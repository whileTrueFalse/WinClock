// ---------------------------------------------------------------- extra panels under the clocks
// Answer cards (pinned calculator results), focus timer + stopwatch, market hours, day/night map.
// Plus the sky helpers used in tooltips: moon phase, prayer times, and the weather cache.
#include "landmask.inc"

// ---- answer cards
struct Card { std::wstring title, sub; };
static std::vector<Card> g_cards;
static void SaveCards() {
    WritePrivateProfileSectionW(L"Cards", L"", g_ini.c_str());
    IniPutI(L"Cards", L"Count", (int)g_cards.size());
    for (size_t i = 0; i < g_cards.size(); i++) {
        std::wstring a = g_cards[i].title, b = g_cards[i].sub;
        std::replace(a.begin(), a.end(), L'|', L'/'); std::replace(b.begin(), b.end(), L'|', L'/');
        IniPut(L"Cards", (L"Card" + std::to_wstring(i + 1)).c_str(), a + L"|" + b);
    }
}
static void LoadCards() {
    g_cards.clear(); int n = std::min(IniInt(L"Cards", L"Count", 0), 6);
    for (int i = 1; i <= n; i++) {
        std::wstring v = IniStr(L"Cards", (L"Card" + std::to_wstring(i)).c_str(), L""); size_t p = v.find(L'|');
        if (p != std::wstring::npos) g_cards.push_back({ v.substr(0, p), v.substr(p + 1) });
    }
}
static void PinCard(const std::wstring& title, const std::wstring& sub) {
    g_cards.insert(g_cards.begin(), { title, sub });
    if (g_cards.size() > 3) g_cards.resize(3);
    SaveCards();
}

// ---- focus timer (pomodoro) and stopwatch
struct Focus { int phase = 0; ULONGLONG endTick = 0, leftMs = 0; bool paused = false; int done = 0; };   // phase 0 off, 1 focus, 2 break, 3 long break
static Focus g_focus;
struct Stopwatch { bool running = false; ULONGLONG startTick = 0, accMs = 0; std::vector<ULONGLONG> laps; };
static Stopwatch g_sw;
static bool TimerActive() { return g_focus.phase != 0 || g_sw.running || g_sw.accMs > 0; }
static ULONGLONG FocusLeft() { return g_focus.phase == 0 ? 0 : g_focus.paused ? g_focus.leftMs : (g_focus.endTick > GetTickCount64() ? g_focus.endTick - GetTickCount64() : 0); }
static ULONGLONG SwElapsed() { return g_sw.accMs + (g_sw.running ? GetTickCount64() - g_sw.startTick : 0); }
static std::wstring MsClock(ULONGLONG ms, bool tenths = false) {
    ULONGLONG s = ms / 1000;
    std::wstring t = s >= 3600 ? Fmt(L"%llu:%02llu:%02llu", s / 3600, (s / 60) % 60, s % 60) : Fmt(L"%02llu:%02llu", s / 60, s % 60);
    if (tenths) t += Fmt(L".%llu", (ms / 100) % 10);
    return t;
}
static const wchar_t* FocusPhaseName(int p) { return p == 1 ? L"focus" : p == 2 ? L"break" : p == 3 ? L"long break" : L""; }
static void FocusStart(int phase, int minutes = 0) {
    if (!minutes) minutes = phase == 1 ? S.focusMin : phase == 2 ? S.breakMin : S.longMin;
    g_focus.phase = phase; g_focus.paused = false; g_focus.endTick = GetTickCount64() + (ULONGLONG)minutes * 60000; g_focus.leftMs = 0;
}
static void FocusStop() { g_focus = Focus{}; }
static void FocusPause() {
    if (!g_focus.phase) return;
    if (g_focus.paused) { g_focus.endTick = GetTickCount64() + g_focus.leftMs; g_focus.paused = false; }
    else { g_focus.leftMs = FocusLeft(); g_focus.paused = true; }
}
static void PlayAlert();
static void FocusTick() {   // called every second while a timer runs
    if (!g_focus.phase || g_focus.paused || FocusLeft() > 0) return;
    int was = g_focus.phase;
    if (was == 1) {
        g_focus.done++;
        bool longB = g_focus.done % 4 == 0;
        FocusStart(longB ? 3 : 2);
        Balloon(L"Focus session done", Fmt(L"Nice work: %d done today. %ls for %d min.", g_focus.done, longB ? L"Long break" : L"Break", longB ? S.longMin : S.breakMin));
    } else {
        FocusStart(1);
        Balloon(L"Break's over", Fmt(L"Next focus session: %d min.", S.focusMin));
    }
    PlayAlert();
}
static void SwToggle() {
    if (g_sw.running) { g_sw.accMs = SwElapsed(); g_sw.running = false; }
    else { g_sw.startTick = GetTickCount64(); g_sw.running = true; }
}
static void SwLap() { if (g_sw.running || g_sw.accMs) { g_sw.laps.push_back(SwElapsed()); if (g_sw.laps.size() > 99) g_sw.laps.erase(g_sw.laps.begin()); } }
static void SwReset() { g_sw = Stopwatch{}; }
static std::wstring SwLapsText() {
    std::wstring s; ULONGLONG prev = 0;
    for (size_t i = 0; i < g_sw.laps.size(); i++) { s += Fmt(L"Lap %zu  %ls  (+%ls)\r\n", i + 1, MsClock(g_sw.laps[i], true).c_str(), MsClock(g_sw.laps[i] - prev, true).c_str()); prev = g_sw.laps[i]; }
    s += L"Total  " + MsClock(SwElapsed(), true);
    return s;
}
// the text shown in the taskbar strip while a timer runs
static std::wstring TimerBarText() {
    if (g_focus.phase) return (g_focus.paused ? L"❚❚ " : g_focus.phase == 1 ? L"● " : L"○ ") + MsClock(FocusLeft());
    if (g_sw.running || g_sw.accMs) return (g_sw.running ? L"⏱ " : L"❚❚ ") + MsClock(SwElapsed());
    return L"";
}
static void TimerBarClick() { if (g_focus.phase) FocusPause(); else if (g_sw.running || g_sw.accMs) SwToggle(); }

// ---- market hours (approximate: regular sessions, weekends and national public holidays; no half days)
struct Market { const wchar_t* key; const wchar_t* name; const wchar_t* tz; const wchar_t* cc; int o1, c1, o2, c2; int weekendMask; };
#define HM(h, m) ((h) * 60 + (m))
static const Market kMarkets[] = {
    { L"NSE",    L"NSE / BSE (India)",      L"India Standard Time",            L"IN", HM(9, 15), HM(15, 30), 0, 0, 0x41 },
    { L"NYSE",   L"NYSE / Nasdaq",          L"Eastern Standard Time",          L"US", HM(9, 30), HM(16, 0),  0, 0, 0x41 },
    { L"LSE",    L"London (LSE)",           L"GMT Standard Time",              L"GB", HM(8, 0),  HM(16, 30), 0, 0, 0x41 },
    { L"XETRA",  L"Frankfurt (Xetra)",      L"W. Europe Standard Time",        L"DE", HM(9, 0),  HM(17, 30), 0, 0, 0x41 },
    { L"EPA",    L"Euronext Paris",         L"Romance Standard Time",          L"FR", HM(9, 0),  HM(17, 30), 0, 0, 0x41 },
    { L"TSE",    L"Tokyo (TSE)",            L"Tokyo Standard Time",            L"JP", HM(9, 0),  HM(11, 30), HM(12, 30), HM(15, 30), 0x41 },
    { L"HKEX",   L"Hong Kong (HKEX)",       L"China Standard Time",            L"",   HM(9, 30), HM(12, 0),  HM(13, 0),  HM(16, 0),  0x41 },
    { L"SSE",    L"Shanghai (SSE)",         L"China Standard Time",            L"CN", HM(9, 30), HM(11, 30), HM(13, 0),  HM(15, 0),  0x41 },
    { L"SGX",    L"Singapore (SGX)",        L"Singapore Standard Time",        L"SG", HM(9, 0),  HM(12, 0),  HM(13, 0),  HM(17, 0),  0x41 },
    { L"ASX",    L"Sydney (ASX)",           L"AUS Eastern Standard Time",      L"AU", HM(10, 0), HM(16, 0),  0, 0, 0x41 },
    { L"TSX",    L"Toronto (TSX)",          L"Eastern Standard Time",          L"CA", HM(9, 30), HM(16, 0),  0, 0, 0x41 },
    { L"B3",     L"São Paulo (B3)",         L"E. South America Standard Time", L"BR", HM(10, 0), HM(17, 0),  0, 0, 0x41 },
    { L"TADAWUL",L"Saudi (Tadawul)",        L"Arab Standard Time",             L"",   HM(10, 0), HM(15, 0),  0, 0, 0x60 },
    { L"DFM",    L"Dubai (DFM)",            L"Arabian Standard Time",          L"AE", HM(10, 0), HM(15, 0),  0, 0, 0x41 },
};
#undef HM
static const int kMarketCount = (int)(sizeof(kMarkets) / sizeof(*kMarkets));
static bool MarketOn(int i) { return (L"," + S.markets + L",").find(std::wstring(L",") + kMarkets[i].key + L",") != std::wstring::npos; }
static void MarketToggle(int i) {
    std::vector<std::wstring> keep;
    for (int k = 0; k < kMarketCount; k++) if ((k == i) != MarketOn(k)) keep.push_back(kMarkets[k].key);
    S.markets.clear(); for (auto& k : keep) S.markets += (S.markets.empty() ? L"" : L",") + k;
}
// sessions of market m on the local calendar day `day`, as UTC minutes
static int MarketSessions(const Market& m, const Clock& c, long long day, long long out[4]) {
    if (m.weekendMask & (1 << WeekdayOf(day))) return 0;
    if (m.cc[0] && HolidayOn(m.cc, day)) return 0;
    SYSTEMTIME d = DateOf(day); int n = 0;
    int pts[4] = { m.o1, m.c1, m.o2, m.c2 };
    for (int k = 0; k < (m.o2 ? 4 : 2); k++) { SYSTEMTIME t = d; t.wHour = (WORD)(pts[k] / 60); t.wMinute = (WORD)(pts[k] % 60); if (!LocalToUtcMin(c, t, out[k])) return 0; n++; }
    return n;
}
// 1 = open, 0 = closed, 2 = lunch break; `change` = UTC minute of the next open/close
static int MarketState(const Market& m, long long u, long long& change) {
    Clock c = ZoneClock(m.tz);
    CTime t = ClockAt(c, (ULONGLONG)u * TPM);
    long long today = DaysOf(t.st.wYear, t.st.wMonth, t.st.wDay);
    for (long long d = today; d < today + 10; d++) {
        long long s[4]; int n = MarketSessions(m, c, d, s);
        for (int k = 0; k + 1 < n; k += 2) {
            if (u < s[k]) { change = s[k]; return (k == 2 && u >= s[1]) ? 2 : 0; }
            if (u < s[k + 1]) { change = s[k + 1]; return 1; }
        }
    }
    change = -1; return 0;
}

// ---- moon phase (good to a few hours)
static double MoonAgeDays(long long u) {
    double jd = u / 1440.0 + 2305813.5;
    double age = fmod(jd - 2451550.1, 29.530588853); if (age < 0) age += 29.530588853;
    return age;
}
static std::wstring MoonText(long long u) {
    double age = MoonAgeDays(u), illum = (1 - cos(age / 29.530588853 * 2 * 3.14159265358979)) / 2;
    static const wchar_t* names[] = { L"New moon", L"Waxing crescent", L"First quarter", L"Waxing gibbous", L"Full moon", L"Waning gibbous", L"Last quarter", L"Waning crescent" };
    int idx = (int)floor(age / 29.530588853 * 8 + 0.5) % 8;
    return Fmt(L"%ls, %d%% lit", names[idx], (int)llround(illum * 100));
}

// ---- prayer times (standard astronomical methods; Asr standard or Hanafi)
struct PrayerMethod { const wchar_t* name; double fajr, isha; int ishaMin; };
static const PrayerMethod kPrayerMethods[] = {
    { L"Muslim World League", 18, 17, 0 }, { L"ISNA (North America)", 15, 15, 0 }, { L"Egyptian Authority", 19.5, 17.5, 0 },
    { L"Umm al-Qura (Makkah)", 18.5, 0, 90 }, { L"Karachi (Islamic Sciences)", 18, 18, 0 },
};
static const int kPrayerMethodCount = 5;
static bool SolarDeclEq(int y, int m, int d, double& decl, double& eqTime) {
    const double PI = 3.14159265358979323846, R = PI / 180;
    double jd = (double)DaysOf(y, m, d) + 2305813.5 + 0.5, t = (jd - 2451545.0) / 36525.0;
    double L0 = fmod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0), M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    double C = sin(M * R) * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin(2 * M * R) * (0.019993 - 0.000101 * t) + sin(3 * M * R) * 0.000289;
    double omega = 125.04 - 1934.136 * t, lambda = L0 + C - 0.00569 - 0.00478 * sin(omega * R);
    double eps = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0 + 0.00256 * cos(omega * R);
    decl = asin(sin(eps * R) * sin(lambda * R));
    double yv = tan(eps * R / 2); yv *= yv;
    eqTime = 4 / R * (yv * sin(2 * L0 * R) - 2 * e * sin(M * R) + 4 * e * yv * sin(M * R) * cos(2 * L0 * R) - 0.5 * yv * yv * sin(4 * L0 * R) - 1.25 * e * e * sin(2 * M * R));
    return true;
}
// minutes after UTC midnight of the given date; NAN where the sun never reaches that angle
static void PrayerTimesUtc(int y, int m, int d, double lat, double lon, int method, bool hanafi, double out[6]) {
    const double R = 3.14159265358979323846 / 180;
    double decl, eq; SolarDeclEq(y, m, d, decl, eq);
    double noon = 720 - 4 * lon - eq;
    auto ha = [&](double altDeg) { double c = (sin(altDeg * R) - sin(lat * R) * sin(decl)) / (cos(lat * R) * cos(decl)); return (c < -1 || c > 1) ? NAN : acos(c) / R; };
    const PrayerMethod& pm = kPrayerMethods[std::clamp(method, 0, kPrayerMethodCount - 1)];
    double sunH = ha(-0.833);
    out[0] = noon - 4 * ha(-pm.fajr);                                  // Fajr
    out[1] = noon - 4 * sunH;                                          // Sunrise
    out[2] = noon + 1;                                                 // Dhuhr
    double asrAlt = atan(1.0 / ((hanafi ? 2 : 1) + tan(fabs(lat * R - decl)))) / R;
    out[3] = noon + 4 * ha(asrAlt);                                    // Asr
    out[4] = noon + 4 * sunH;                                          // Maghrib
    out[5] = pm.ishaMin ? out[4] + pm.ishaMin : noon + 4 * ha(-pm.isha);   // Isha
}
static std::wstring PrayerText(const Clock& c, long long u) {
    if (std::isnan(c.lat) || std::isnan(c.lon)) return L"";
    CTime t = ClockAt(c, (ULONGLONG)u * TPM);
    double p[6]; PrayerTimesUtc(t.st.wYear, t.st.wMonth, t.st.wDay, c.lat, c.lon, S.prayerMethod, S.asrHanafi, p);
    static const wchar_t* nm[] = { L"Fajr", L"Sunrise", L"Dhuhr", L"Asr", L"Maghrib", L"Isha" };
    long long base = DaysOf(t.st.wYear, t.st.wMonth, t.st.wDay) * 1440;
    std::wstring s, next;
    for (int k = 0; k < 6; k++) {
        if (std::isnan(p[k])) continue;
        long long at = base + (long long)llround(p[k]);
        // the UTC minute belongs to the city's civil date: nudge by a day if the offset pushed it out
        CTime lt = ClockAt(c, (ULONGLONG)at * TPM);
        long long dd = DaysOf(lt.st.wYear, lt.st.wMonth, lt.st.wDay) - base / 1440; at -= dd * 1440; lt = ClockAt(c, (ULONGLONG)at * TPM);
        if (k == 1) continue;   // sunrise is already in the tooltip
        s += (s.empty() ? L"" : L" · ") + std::wstring(nm[k]) + L" " + Lower(FmtHMFull(lt.st));
        if (next.empty() && at > u) next = std::wstring(nm[k]) + L" in " + FmtSpan(at - u);
    }
    return s.empty() ? L"" : L"Prayer: " + s + (next.empty() ? L"" : L" (" + next + L")");
}

// ---- weather cache (filled by net.inc when the weather option is on)
struct Weather { float tempC = NAN; int code = -1; long long at = -1; };
static std::map<std::wstring, Weather> g_weather;   // key "lat,lon"
static std::wstring WxKey(const Clock& c) { return std::isnan(c.lat) ? L"" : Fmt(L"%.2f,%.2f", c.lat, c.lon); }
static const Weather* WeatherFor(const Clock& c) {
    if (!S.weather) return nullptr;
    auto it = g_weather.find(WxKey(c)); if (it == g_weather.end() || it->second.code < 0) return nullptr;
    if (NowMin() - it->second.at > 180) return nullptr;   // stale
    return &it->second;
}
static int WxKind(int code) {   // 0 clear, 1 partly, 2 cloudy, 3 fog, 4 rain, 5 snow, 6 storm
    if (code == 0) return 0; if (code <= 2) return 1; if (code == 3) return 2; if (code == 45 || code == 48) return 3;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 5; if (code >= 95) return 6; return 4;
}
static const wchar_t* WxName(int code) {
    static const wchar_t* n[] = { L"clear", L"partly cloudy", L"cloudy", L"fog", L"rain", L"snow", L"thunderstorm" };
    return n[WxKind(code)];
}
static std::wstring WxTemp(const Weather& w) { return S.weatherF ? Fmt(L"%d°F", (int)llround(w.tempC * 9 / 5 + 32)) : Fmt(L"%d°", (int)llround(w.tempC)); }
static void DrawWxIcon(G::Graphics& g, int code, bool night, float cx, float cy, float r, DWORD col, DWORD accent) {
    int k = WxKind(code);
    G::Pen p{ G::Color(col), std::max(1.f, r * 0.16f) }; p.SetLineCap(G::LineCapRound, G::LineCapRound, G::DashCapRound);
    G::SolidBrush b{ G::Color(col) };
    auto sun = [&](float x, float y, float rr) {
        if (night) {   // crescent moon
            G::GraphicsPath gp; gp.AddEllipse(x - rr, y - rr, 2 * rr, 2 * rr);
            G::Region rg(&gp); G::GraphicsPath cut; cut.AddEllipse(x - rr * 0.35f, y - rr * 1.25f, 2 * rr, 2 * rr); rg.Exclude(&cut);
            G::SolidBrush ab{ G::Color(accent) }; g.FillRegion(&ab, &rg); return;
        }
        G::SolidBrush ab{ G::Color(accent) }; g.FillEllipse(&ab, x - rr * 0.55f, y - rr * 0.55f, rr * 1.1f, rr * 1.1f);
        G::Pen ap{ G::Color(accent), std::max(1.f, rr * 0.18f) }; ap.SetLineCap(G::LineCapRound, G::LineCapRound, G::DashCapRound);
        for (int i = 0; i < 8; i++) { float a = i * 3.14159f / 4; g.DrawLine(&ap, x + cosf(a) * rr * 0.8f, y + sinf(a) * rr * 0.8f, x + cosf(a) * rr * 1.05f, y + sinf(a) * rr * 1.05f); }
    };
    auto cloud = [&](float x, float y, float rr) {
        G::GraphicsPath gp;
        gp.AddEllipse(x - rr, y - rr * 0.1f, rr * 1.0f, rr * 0.75f);
        gp.AddEllipse(x - rr * 0.55f, y - rr * 0.6f, rr * 1.05f, rr * 1.05f);
        gp.AddEllipse(x + rr * 0.05f, y - rr * 0.25f, rr * 0.95f, rr * 0.85f);
        gp.AddRectangle(G::RectF(x - rr * 0.6f, y + rr * 0.1f, rr * 1.2f, rr * 0.55f));
        G::Region rg(&gp); g.FillRegion(&b, &rg);
    };
    switch (k) {
    case 0: sun(cx, cy, r); break;
    case 1: sun(cx - r * 0.35f, cy - r * 0.3f, r * 0.75f); cloud(cx + r * 0.15f, cy + r * 0.15f, r * 0.8f); break;
    case 2: cloud(cx, cy, r); break;
    case 3: for (int i = 0; i < 3; i++) g.DrawLine(&p, cx - r, cy - r * 0.5f + i * r * 0.5f, cx + r, cy - r * 0.5f + i * r * 0.5f); break;
    case 4: cloud(cx, cy - r * 0.25f, r * 0.85f); for (int i = -1; i <= 1; i++) g.DrawLine(&p, cx + i * r * 0.45f, cy + r * 0.55f, cx + i * r * 0.45f - r * 0.15f, cy + r * 0.95f); break;
    case 5: cloud(cx, cy - r * 0.25f, r * 0.85f); for (int i = -1; i <= 1; i++) g.FillEllipse(&b, cx + i * r * 0.45f - r * 0.1f, cy + r * 0.65f, r * 0.2f, r * 0.2f); break;
    default: { cloud(cx, cy - r * 0.25f, r * 0.85f); G::Pen ap{ G::Color(accent), std::max(1.f, r * 0.18f) };
               G::PointF z[] = { { cx + r * 0.1f, cy + r * 0.35f }, { cx - r * 0.2f, cy + r * 0.7f }, { cx + r * 0.1f, cy + r * 0.7f }, { cx - r * 0.15f, cy + r * 1.05f } };
               g.DrawLines(&ap, z, 4); }
    }
}

// ---- day/night map
static bool LandAt(int row, int col) { return (kLand[row][col >> 3] >> (col & 7)) & 1; }
static double SolarAltitude(long long u, double lat, double lon) {   // degrees
    SYSTEMTIME st = StOfMin(u); double decl, eq; SolarDeclEq(st.wYear, st.wMonth, st.wDay, decl, eq);
    double minutes = st.wHour * 60 + st.wMinute;
    double ha = (minutes + eq + 4 * lon) / 4 - 180;   // hour angle, degrees
    const double R = 3.14159265358979323846 / 180;
    return asin(sin(lat * R) * sin(decl) + cos(lat * R) * cos(decl) * cos(ha * R)) / R;
}

// ---- layout of the panels
static int MarketsShown() { int n = 0; for (int i = 0; i < kMarketCount; i++) if (MarketOn(i)) n++; return n; }
static float CardsH() { return g_cards.empty() ? 0.f : 8.f + 40.f * g_cards.size(); }
static float TimerH() { return TimerActive() ? 8.f + 34.f + (g_sw.laps.empty() || g_focus.phase ? 0.f : 18.f) : 0.f; }
static float MarketsH() { int n = MarketsShown(); return n ? 10.f + 22.f * ((n + 1) / 2) : 0.f; }
static float MapH(float cw) { return S.showMap ? 10.f + std::max(70.f, cw * 66.f / 180.f) : 0.f; }
static float ExtrasH(float cw) { return CardsH() + TimerH() + MarketsH() + MapH(cw) + PinsH(); }

static void DrawPanels(Ctx& c, float y) {   // y = top of the area, scaled
    G::Graphics& g = c.g; float s = c.s, x0 = c.x0, x1 = c.x1; const ThemeC& th = c.th;
    auto sep = [&](float yy) { G::Pen sp{ G::Color(th.sep), 1 }; g.DrawLine(&sp, x0, yy, x1, yy); };
    long long now = NowMin(), u = now + g_shift;
    // answer cards
    if (!g_cards.empty()) {
        sep(y + 4 * s); y += 8 * s;
        for (size_t k = 0; k < g_cards.size(); k++) {
            float cy = y + k * 40 * s;
            c.v.hits.push_back({ { (LONG)x0, (LONG)cy, (LONG)x1, (LONG)(cy + 38 * s) }, -200 - (int)k });
            FillRR(g, x0 - 4 * s, cy + 2 * s, x1 - x0 + 8 * s, 35 * s, 5 * s, c.v.hover == -200 - (int)k ? th.hover : A(th.accent, 0x18));
            G::SolidBrush ab{ G::Color(th.accent) }; g.FillRectangle(&ab, x0 - 4 * s, cy + 6 * s, 3 * s, 27 * s);
            DT(g, g_cards[k].title, F(FS, 13.5f * s), x0 + 6 * s, cy + 17 * s, th.fg, AL, x1 - x0 - 30 * s, th.shadow);
            DT(g, g_cards[k].sub, F(FR, 11 * s), x0 + 6 * s, cy + 31 * s, th.dim, AL, x1 - x0 - 30 * s, th.shadow);
            DT(g, L"×", F(FS, 14 * s), x1 - 2 * s, cy + 18 * s, c.v.hover == -200 - (int)k ? th.accent : th.dim, AR);
        }
        y += 40 * s * g_cards.size();
    }
    // focus timer / stopwatch
    if (TimerActive()) {
        sep(y + 4 * s); y += 8 * s;
        c.v.hits.push_back({ { (LONG)x0, (LONG)y, (LONG)x1, (LONG)(y + 34 * s) }, -300 });
        std::wstring big, label, right;
        if (g_focus.phase) {
            big = MsClock(FocusLeft()); label = std::wstring(FocusPhaseName(g_focus.phase)) + (g_focus.paused ? L" · paused" : L"");
            right = Fmt(L"%d done · click to %ls", g_focus.done, g_focus.paused ? L"resume" : L"pause");
            int total = (g_focus.phase == 1 ? S.focusMin : g_focus.phase == 2 ? S.breakMin : S.longMin) * 60000;
            float frac = 1.f - std::clamp((float)FocusLeft() / std::max(1, total), 0.f, 1.f);
            FillRR(g, x0, y + 29 * s, x1 - x0, 3 * s, 1.5f * s, A(th.dim, 0x40));
            FillRR(g, x0, y + 29 * s, (x1 - x0) * frac, 3 * s, 1.5f * s, g_focus.phase == 1 ? th.accent : th.blue);
        } else {
            big = MsClock(SwElapsed(), true); label = g_sw.running ? L"stopwatch" : L"stopwatch · paused";
            right = g_sw.laps.empty() ? std::wstring(L"click to ") + (g_sw.running ? L"pause" : L"resume") : Fmt(L"%zu laps", g_sw.laps.size());
        }
        DT(g, big, F(FMS, 20 * s), x0, y + 22 * s, g_focus.phase == 1 ? th.accent : th.fg, AL, 0, th.shadow);
        float bw = TW(g, big, F(FMS, 20 * s));
        DT(g, label, F(FS, 12.5f * s), x0 + bw + 12 * s, y + 21 * s, th.dim, AL, 0, th.shadow);
        DT(g, right, F(FR, 11.5f * s), x1, y + 21 * s, th.dim, AR, 0, th.shadow);
        y += 34 * s;
        if (!g_focus.phase && !g_sw.laps.empty()) {
            std::wstring l; ULONGLONG prev = g_sw.laps.size() >= 2 ? g_sw.laps[g_sw.laps.size() - 2] : 0;
            l = Fmt(L"lap %zu  %ls  (+%ls)", g_sw.laps.size(), MsClock(g_sw.laps.back(), true).c_str(), MsClock(g_sw.laps.back() - prev, true).c_str());
            DT(g, l, F(FM, 11.5f * s), x0, y + 12 * s, th.dim, AL, 0, th.shadow);
            y += 18 * s;
        }
    }
    // markets
    if (MarketsShown()) {
        sep(y + 4 * s); y += 10 * s;
        int k = 0; float colW = (x1 - x0) / 2;
        for (int i = 0; i < kMarketCount; i++) {
            if (!MarketOn(i)) continue;
            float mx = x0 + (k % 2) * colW, my = y + (k / 2) * 22 * s + 15 * s; k++;
            long long ch; int st = MarketState(kMarkets[i], u, ch);
            float r = 3.6f * s;
            if (st == 1) { G::SolidBrush b{ G::Color(th.accent) }; g.FillEllipse(&b, mx, my - 4 * s - r, 2 * r, 2 * r); }
            else { G::Pen p{ G::Color(st == 2 ? th.blue : A(th.dim, 0xA0)), 1.4f * s }; g.DrawEllipse(&p, mx, my - 4 * s - r, 2 * r, 2 * r); }
            DT(g, kMarkets[i].key, F(FMS, 12 * s), mx + 12 * s, my, th.fg, AL, 0, th.shadow);
            float kw = TW(g, kMarkets[i].key, F(FMS, 12 * s));
            std::wstring stx = st == 1 ? L"open · closes in " : st == 2 ? L"lunch · reopens in " : L"closed · opens in ";
            stx += ch >= 0 ? FmtSpan(ch - u) : std::wstring(L"?");
            DT(g, stx, F(FR, 11.5f * s), mx + 12 * s + kw + 8 * s, my, st == 1 ? th.accent : th.dim, AL, colW - kw - 26 * s, th.shadow);
        }
        y += 22 * s * ((k + 1) / 2);
    }
    // day/night map
    if (S.showMap) {
        sep(y + 4 * s); y += 10 * s;
        float mw = x1 - x0, mh = std::max(70.f * s, mw * 66.f / 180.f), cw = mw / 180.f, chh = mh / 66.f;
        float dot = std::max(1.f, std::min(cw, chh) * 0.62f);
        G::SolidBrush land{ G::Color(A(th.fg, 0x96)) }, landT{ G::Color(A(th.fg, 0x52)) }, landN{ G::Color(A(th.fg, 0x1E)) }, sea{ G::Color(A(th.blue, 0x16)) };
        for (int r = 0; r < 66; r++) {
            double lat = 76 - r * 2 - 1;
            for (int col = 0; col < 180; col++) {
                double lon = -180 + col * 2 + 1;
                bool isLand = LandAt(r, col);
                double alt = SolarAltitude(u, lat, lon);
                float px = x0 + col * cw + cw / 2, py = y + r * chh + chh / 2;
                if (isLand) g.FillEllipse(alt > 0 ? &land : alt > -6 ? &landT : &landN, px - dot / 2, py - dot / 2, dot, dot);
                else if (alt > 0 && (col + r) % 2 == 0) g.FillRectangle(&sea, px - dot / 4, py - dot / 4, dot / 2, dot / 2);
            }
        }
        // the sun
        SYSTEMTIME ust = StOfMin(u); double decl, eq; SolarDeclEq(ust.wYear, ust.wMonth, ust.wDay, decl, eq);
        double sunLon = -((ust.wHour * 60 + ust.wMinute + eq) / 4 - 180), sunLat = decl * 180 / 3.14159265358979;
        while (sunLon < -180) sunLon += 360; while (sunLon > 180) sunLon -= 360;
        float sx = x0 + (float)((sunLon + 180) / 360 * mw), sy = y + (float)((76 - sunLat) / 132 * mh);
        { G::SolidBrush sb{ G::Color(A(th.accent, 0x50)) }; g.FillEllipse(&sb, sx - 7 * s, sy - 7 * s, 14 * s, 14 * s);
          G::SolidBrush sb2{ G::Color(th.accent) }; g.FillEllipse(&sb2, sx - 3.5f * s, sy - 3.5f * s, 7 * s, 7 * s); }
        // your clocks
        for (size_t i = 0; i < g_clocks.size(); i++) {
            const Clock& ck = g_clocks[i]; if (std::isnan(ck.lat)) continue;
            float px = x0 + (float)((ck.lon + 180) / 360 * mw), py = y + (float)((76 - ck.lat) / 132 * mh);
            if (py < y || py > y + mh) continue;
            G::SolidBrush pb{ G::Color(th.fg) }; g.FillEllipse(&pb, px - 3 * s, py - 3 * s, 6 * s, 6 * s);
            G::Pen pp{ G::Color(th.bg | 0xFF000000), 1.2f * s }; g.DrawEllipse(&pp, px - 3 * s, py - 3 * s, 6 * s, 6 * s);
            std::wstring lab = ck.label; FontE* fl = F(FS, 10.5f * s); float lw = TW(g, lab, fl);
            float lx = px + 6 * s; int al = AL; if (lx + lw > x1) { lx = px - 6 * s; al = AR; }
            DT(g, lab, fl, lx, py + 4 * s, th.fg, al, 0, true);
            c.v.hits.push_back({ { (LONG)(px - 6 * s), (LONG)(py - 6 * s), (LONG)(px + 6 * s), (LONG)(py + 6 * s) }, (int)i });
        }
        y += mh;
    }
}

// ---- tooltip lines: weather, moon, prayer times
static std::wstring TooltipExtras(const Clock& c, long long u) {
    std::wstring s;
    if (const Weather* w = WeatherFor(c)) s += L"Weather: " + WxTemp(*w) + (S.weatherF ? L"" : L"C") + L", " + WxName(w->code) + Fmt(L" (updated %lld min ago)", NowMin() - w->at) + L"\n";
    s += L"Moon: " + MoonText(u) + L"\n";
    if (S.prayer) { std::wstring p = PrayerText(c, u); if (!p.empty()) s += p + L"\n"; }
    return s;
}

// ---------------------------------------------------------------- weather (opt-in, Open-Meteo: free, no key, no account)
static std::vector<std::wstring> g_wxKeys;   // order of the last request
static void WeatherTick(bool force = false) {
    static long long last = -1000;
    if (!S.weather) return;
    long long now = NowMin();
    if (!force && now - last < 30) return;
    std::wstring lats, lons; g_wxKeys.clear();
    for (auto& c : g_clocks) {
        std::wstring k = WxKey(c); if (k.empty() || std::find(g_wxKeys.begin(), g_wxKeys.end(), k) != g_wxKeys.end() || g_wxKeys.size() >= 25) continue;
        g_wxKeys.push_back(k);
        lats += (lats.empty() ? L"" : L",") + Fmt(L"%.2f", c.lat); lons += (lons.empty() ? L"" : L",") + Fmt(L"%.2f", c.lon);
    }
    if (g_wxKeys.empty()) return;
    last = now;
    NetStart(NET_WEATHER, L"https://api.open-meteo.com/v1/forecast?latitude=" + lats + L"&longitude=" + lons + L"&current=temperature_2m,weather_code&timezone=GMT");
}
static void ParseWeather(const std::string& body) {
    size_t p = 0; long long now = NowMin();
    for (size_t k = 0; k < g_wxKeys.size(); k++) {
        p = body.find("\"current\":{", p); if (p == std::string::npos) break;
        size_t e = body.find('}', p); std::string blk = body.substr(p, e - p);
        auto num = [&](const char* key) -> double { size_t q = blk.find(std::string("\"") + key + "\":"); if (q == std::string::npos) return NAN; return atof(blk.c_str() + q + strlen(key) + 3); };
        Weather w; w.tempC = (float)num("temperature_2m"); double code = num("weather_code"); w.code = std::isnan(code) ? -1 : (int)code; w.at = now;
        if (!std::isnan(w.tempC) && w.code >= 0) g_weather[g_wxKeys[k]] = w;
        p = e;
    }
}

// ---- mini strip: one line, about taskbar height ("London 11:15 am · Frankfurt 12:15 pm · ...")
static void PaintStrip(View& v, float s, int theme, Frame& fr) {
    ThemeC th = GetTheme(theme);
    FontE* fl = F(FS, 12 * s); FontE* ft = F(FMS, 14 * s); FontE* fa = F(FM, 11 * s);
    struct Cell { std::wstring lab, hm, ap; float w; int st; int dd; };
    std::vector<Cell> cells; float total = 12 * s;
    {
        HDC sdc = GetDC(nullptr); G::Graphics mg(sdc);
        for (size_t i = 0; i < g_clocks.size(); i++) {
            Cell c; const CTime& t = fr.t[i]; c.lab = g_clocks[i].label; c.hm = FmtHM(t.st, &c.ap); c.ap = Lower(c.ap);
            c.st = ClockStatusAt(g_clocks[i], t.st); c.dd = (int)(DayNum(t.st) - DayNum(fr.local.st));
            c.w = 14 * s + TW(mg, c.lab, fl) + 7 * s + TW(mg, c.hm, ft) + (c.ap.empty() ? 0 : 3 * s + TW(mg, c.ap, fa)) + (c.dd ? 26 * s : 0) + 18 * s;
            total += c.w; cells.push_back(c);
        }
        std::wstring tm = TimerBarText(); if (!tm.empty()) total += TW(mg, tm, ft) + 24 * s;
        ReleaseDC(nullptr, sdc);
    }
    if (cells.empty()) total = 220 * s;
    int w = (int)ceilf(total), h = (int)ceilf(34 * s);
    EnsureSurface(v.surf, w, h); v.hits.clear(); v.tlBar = RECT{}; v.flashRc = RECT{};
    {
        G::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)v.surf.bits);
        G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0, 0, 0, 0));
        if ((th.bg >> 24) >= 0xF0) g.SetTextRenderingHint(G::TextRenderingHintClearTypeGridFit);
        { G::GraphicsPath p; RoundRect(p, 0.5f, 0.5f, w - 1.f, h - 1.f, 8 * s); G::SolidBrush b{ G::Color(th.bg) }; g.FillPath(&b, &p);
          G::Pen pen{ G::Color(th.border), 1 }; g.DrawPath(&pen, &p); }
        float x = 12 * s, base = h / 2.f + 5 * s;
        std::wstring tm = TimerBarText();
        if (!tm.empty()) {
            float tw = TW(g, tm, ft);
            FillRR(g, x - 4 * s, 6 * s, tw + 12 * s, h - 12 * s, 5 * s, g_focus.phase == 1 ? A(th.accent, 0x40) : A(th.blue, 0x40));
            DT(g, tm, ft, x + 2 * s, base, th.fg, AL, 0, th.shadow);
            v.hits.push_back({ { (LONG)(x - 4 * s), 0, (LONG)(x + tw + 8 * s), (LONG)h }, -300 });
            x += tw + 24 * s;
        }
        if (cells.empty()) DT(g, L"right-click → add clocks", fl, x, base, th.dim, AL);
        for (size_t i = 0; i < cells.size(); i++) {
            const Cell& c = cells[i]; float x0 = x;
            if ((int)i == v.hover) FillRR(g, x - 6 * s, 4 * s, c.w - 4 * s, h - 8 * s, 5 * s, th.hover);
            SafeDot(g, th, c.st, x + 3 * s, base - 4.5f * s, s * 0.85f); x += 12 * s;
            DT(g, c.lab, fl, x, base, th.dim, AL, 0, th.shadow); x += TW(g, c.lab, fl) + 7 * s;
            DT(g, c.hm, ft, x, base + 0.5f * s, th.fg, AL, 0, th.shadow); x += TW(g, c.hm, ft);
            if (!c.ap.empty()) { DT(g, c.ap, fa, x + 3 * s, base, th.accent, AL, 0, th.shadow); x += 3 * s + TW(g, c.ap, fa); }
            if (c.dd) x += 4 * s + DayPill(g, th, c.dd, x + 4 * s, base, s);
            v.hits.push_back({ { (LONG)(x0 - 6 * s), 0, (LONG)(x0 + c.w - 10 * s), (LONG)h }, (int)i });
            x = x0 + c.w;
            if (i + 1 < cells.size()) { G::SolidBrush db{ G::Color(A(th.dim, 0x70)) }; g.FillEllipse(&db, x - 11 * s, h / 2.f - 1.5f * s, 3 * s, 3 * s); }
        }
    }
    v.W = w; v.H = h;
    Present(v.hwnd, v.surf, v.fly ? v.animAlpha : (BYTE)(S.opacity * 255 / 100), nullptr);
    if (!v.fly) KeepOnScreen(v.hwnd);
    long long secs = (long long)(fr.utc / 10000000ULL);
    v.key = TimerActive() ? secs : secs / 60;
}

// ---- sharing: a time for everyone, and your working hours for a Slack / Teams status
static std::wstring ZoneAbbr(const Clock& c, long long u) {   // "IST", "CET", or "UTC+5:30"
    std::wstring key = c.kind == 1 ? g_localKey : c.tzid;
    CTime t = ClockAt(c, (ULONGLONG)u * TPM);
    struct Ab { const wchar_t* tz; const wchar_t* std; const wchar_t* dst; };
    static const Ab ab[] = {
        { L"India Standard Time", L"IST", L"IST" }, { L"Eastern Standard Time", L"EST", L"EDT" }, { L"Central Standard Time", L"CST", L"CDT" },
        { L"Mountain Standard Time", L"MST", L"MDT" }, { L"Pacific Standard Time", L"PST", L"PDT" }, { L"GMT Standard Time", L"GMT", L"BST" },
        { L"W. Europe Standard Time", L"CET", L"CEST" }, { L"Romance Standard Time", L"CET", L"CEST" }, { L"Central Europe Standard Time", L"CET", L"CEST" },
        { L"Central European Standard Time", L"CET", L"CEST" }, { L"GTB Standard Time", L"EET", L"EEST" }, { L"FLE Standard Time", L"EET", L"EEST" },
        { L"Tokyo Standard Time", L"JST", L"JST" }, { L"Korea Standard Time", L"KST", L"KST" }, { L"China Standard Time", L"CST (China)", L"CST (China)" },
        { L"Singapore Standard Time", L"SGT", L"SGT" }, { L"AUS Eastern Standard Time", L"AEST", L"AEDT" }, { L"Arabian Standard Time", L"GST", L"GST" },
        { L"Pakistan Standard Time", L"PKT", L"PKT" }, { L"New Zealand Standard Time", L"NZST", L"NZDT" }, { L"UTC", L"UTC", L"UTC" },
    };
    for (auto& a : ab) if (_wcsicmp(a.tz, key.c_str()) == 0) return t.dst ? a.dst : a.std;
    return FmtUtc(t.offset);
}
static std::wstring TimeMessage(const WhenResult& w, const std::vector<PlaceHit>& extra) {
    long long u = w.utc;
    SYSTEMTIME s0 = ZoneTime(w.zone, u);
    std::wstring src = w.zone.name.empty() || w.zone.name == L"your time" ? ZoneAbbr(g_localClock, u) : w.zone.name;
    std::wstring msg = L"\U0001F552 " + FmtHMFull(s0) + L" " + kDays[s0.wDayOfWeek] + Fmt(L" %d %ls", s0.wDay, kMonths[s0.wMonth - 1]) + L" (" + src + L")\r\n";
    std::vector<std::pair<std::wstring, WhenZone>> list;
    for (auto& h : extra) for (auto& z : h.zones) list.push_back({ h.zones.size() > 1 ? h.place + L" (" + z.name + L")" : h.place, z });
    for (size_t i = 0; i < g_clocks.size(); i++) { WhenZone z; z.clock = g_clocks[i]; list.push_back({ g_clocks[i].kind == 1 ? std::wstring(L"Me") : DisplayName((int)i), z }); }
    std::vector<std::wstring> seen;
    long long d0 = DaysOf(s0.wYear, s0.wMonth, s0.wDay);
    for (auto& x : list) {
        SYSTEMTIME st = ZoneTime(x.second, u);
        std::wstring line = x.first + L" — " + FmtHMFull(st) + L" " + kDays[st.wDayOfWeek];
        long long dd = DaysOf(st.wYear, st.wMonth, st.wDay) - d0; if (dd) line += dd > 0 ? L" (next day)" : L" (day before)";
        if (std::find(seen.begin(), seen.end(), x.first) != seen.end()) continue;
        seen.push_back(x.first); msg += L"• " + line + L"\r\n";
    }
    while (!msg.empty() && (msg.back() == L'\n' || msg.back() == L'\r')) msg.pop_back();
    return msg;
}
static std::wstring WorkHoursStatus() {
    long long now = NowMin();
    CTime lt = ClockAt(g_localClock, (ULONGLONG)now * TPM);
    long long day = DaysOf(lt.st.wYear, lt.st.wMonth, lt.st.wDay);
    while (WeekdayOf(day) == 0 || WeekdayOf(day) == 6) day++;   // describe a working day
    SYSTEMTIME a = DateOf(day); a.wHour = (WORD)S.workStart; SYSTEMTIME b = DateOf(day); b.wHour = (WORD)(S.workEnd % 24);
    long long ua, ub; if (!LocalToUtcMin(g_localClock, a, ua) || !LocalToUtcMin(g_localClock, b, ub)) return L"";
    if (ub <= ua) ub += 1440;
    auto hm = [](const SYSTEMTIME& st) { std::wstring s = FmtHMFull(st); return s; };
    std::wstring msg = L"\U0001F558 Working hours: " + hm(a) + L"–" + hm(b) + L" " + ZoneAbbr(g_localClock, ua) + L", Mon–Fri";
    std::wstring others;
    for (size_t i = 0; i < g_clocks.size() && i < 6; i++) {
        if (g_clocks[i].kind == 1) continue;
        SYSTEMTIME x = ClockAt(g_clocks[i], (ULONGLONG)ua * TPM).st, y = ClockAt(g_clocks[i], (ULONGLONG)ub * TPM).st;
        if (x.wHour == a.wHour && x.wMinute == a.wMinute) continue;
        others += (others.empty() ? L"" : L" · ") + hm(x) + L"–" + hm(y) + L" " + g_clocks[i].label;
    }
    if (!others.empty()) msg += L" (= " + others + L")";
    return msg;
}
