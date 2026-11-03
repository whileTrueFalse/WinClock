// ---------------------------------------------------------------- natural-language time parser
// Understands things like "3pm EST Thu", "15:30 London tomorrow", "9:30 am IST on Nov 12",
// "noon PT", "10am UTC+5:30", "3pm alex" (a person on one of your clocks) or "8 pm tokyo time".
// Zone abbreviations are read as "wall clock in that region", so EST in July still means New York time.

struct WhenZone { Clock clock; std::wstring name; int fixedOff = INT_MIN; };
struct WhenResult {
    long long utc = 0;            // absolute minute
    WhenZone zone;                // where the time was expressed
    bool hasTime = false, hasDate = false, hasZone = false;
};

static bool IsWordChar(wchar_t c) { return iswalnum(c) || c == L'_'; }
static bool WordAt(const std::wstring& t, size_t pos, size_t len) {
    if (pos + len > t.size()) return false;
    if (pos > 0 && IsWordChar(t[pos - 1]) && IsWordChar(t[pos])) return false;
    if (pos + len < t.size() && IsWordChar(t[pos + len]) && IsWordChar(t[pos + len - 1])) return false;
    return true;
}
static size_t FindWord(const std::wstring& t, const std::wstring& w, size_t from = 0) {
    for (size_t p = t.find(w, from); p != std::wstring::npos; p = t.find(w, p + 1))
        if (WordAt(t, p, w.size())) return p;
    return std::wstring::npos;
}

struct Abbr { const wchar_t* key; const wchar_t* tz; const wchar_t* name; };
static const Abbr kAbbr[] = {
    { L"est", L"Eastern Standard Time", L"New York" }, { L"edt", L"Eastern Standard Time", L"New York" }, { L"et", L"Eastern Standard Time", L"New York" },
    { L"eastern", L"Eastern Standard Time", L"New York" }, { L"cst", L"Central Standard Time", L"Chicago" }, { L"cdt", L"Central Standard Time", L"Chicago" },
    { L"ct", L"Central Standard Time", L"Chicago" }, { L"central", L"Central Standard Time", L"Chicago" }, { L"mst", L"Mountain Standard Time", L"Denver" },
    { L"mdt", L"Mountain Standard Time", L"Denver" }, { L"mt", L"Mountain Standard Time", L"Denver" }, { L"pst", L"Pacific Standard Time", L"Los Angeles" },
    { L"pdt", L"Pacific Standard Time", L"Los Angeles" }, { L"pt", L"Pacific Standard Time", L"Los Angeles" }, { L"pacific", L"Pacific Standard Time", L"Los Angeles" },
    { L"akst", L"Alaskan Standard Time", L"Anchorage" }, { L"akdt", L"Alaskan Standard Time", L"Anchorage" }, { L"hst", L"Hawaiian Standard Time", L"Honolulu" },
    { L"gmt", L"UTC", L"UTC" }, { L"utc", L"UTC", L"UTC" }, { L"z", L"UTC", L"UTC" }, { L"bst", L"GMT Standard Time", L"London" },
    { L"uk", L"GMT Standard Time", L"London" }, { L"cet", L"W. Europe Standard Time", L"Berlin" }, { L"cest", L"W. Europe Standard Time", L"Berlin" },
    { L"eet", L"GTB Standard Time", L"Athens" }, { L"eest", L"GTB Standard Time", L"Athens" }, { L"wet", L"GMT Standard Time", L"Lisbon" },
    { L"ist", L"India Standard Time", L"India" }, { L"jst", L"Tokyo Standard Time", L"Tokyo" }, { L"kst", L"Korea Standard Time", L"Seoul" },
    { L"sgt", L"Singapore Standard Time", L"Singapore" }, { L"hkt", L"China Standard Time", L"Hong Kong" }, { L"pht", L"Singapore Standard Time", L"Manila" },
    { L"aest", L"AUS Eastern Standard Time", L"Sydney" }, { L"aedt", L"AUS Eastern Standard Time", L"Sydney" }, { L"acst", L"Cen. Australia Standard Time", L"Adelaide" },
    { L"acdt", L"Cen. Australia Standard Time", L"Adelaide" }, { L"awst", L"W. Australia Standard Time", L"Perth" }, { L"nzst", L"New Zealand Standard Time", L"Auckland" },
    { L"nzdt", L"New Zealand Standard Time", L"Auckland" }, { L"msk", L"Russian Standard Time", L"Moscow" }, { L"gst", L"Arabian Standard Time", L"Dubai" },
    { L"pkt", L"Pakistan Standard Time", L"Karachi" }, { L"brt", L"E. South America Standard Time", L"São Paulo" }, { L"art", L"Argentina Standard Time", L"Buenos Aires" },
    { L"wib", L"SE Asia Standard Time", L"Jakarta" }, { L"ict", L"SE Asia Standard Time", L"Bangkok" }, { L"sast", L"South Africa Standard Time", L"Johannesburg" },
    { L"cat", L"South Africa Standard Time", L"Johannesburg" }, { L"eat", L"E. Africa Standard Time", L"Nairobi" }, { L"wat", L"W. Central Africa Standard Time", L"Lagos" },
    { L"ny", L"Eastern Standard Time", L"New York" }, { L"nyc", L"Eastern Standard Time", L"New York" }, { L"sf", L"Pacific Standard Time", L"San Francisco" },
    { L"la", L"Pacific Standard Time", L"Los Angeles" }, { L"dc", L"Eastern Standard Time", L"Washington DC" }, { L"bay area", L"Pacific Standard Time", L"San Francisco" },
    { L"silicon valley", L"Pacific Standard Time", L"San Francisco" }, { L"india", L"India Standard Time", L"India" }, { L"japan", L"Tokyo Standard Time", L"Tokyo" },
    { L"germany", L"W. Europe Standard Time", L"Berlin" }, { L"france", L"Romance Standard Time", L"Paris" }, { L"spain", L"Romance Standard Time", L"Madrid" },
    { L"china", L"China Standard Time", L"Beijing" }, { L"singapore", L"Singapore Standard Time", L"Singapore" }, { L"dubai", L"Arabian Standard Time", L"Dubai" },
    { L"local", L"Local", L"your time" }, { L"my time", L"Local", L"your time" }, { L"your time", L"Local", L"your time" },
};

// finds the zone mentioned in text (longest match wins); clocks and people come first
static bool FindWhenZone(const std::wstring& t, WhenZone& out, size_t& at, size_t& len) {
    size_t bestLen = 0; bool found = false;
    auto consider = [&](const std::wstring& key, auto make) {
        if (key.size() < 2 && key != L"z") return;
        size_t p = FindWord(t, key);
        if (p == std::wstring::npos || key.size() <= bestLen) return;
        if (key == L"z" && (p == 0 || !iswdigit(t[p - 1]))) return;   // "z" only right after digits (12:00Z)
        bestLen = key.size(); at = p; len = key.size(); out = make(); found = true;
    };
    for (size_t i = 0; i < g_clocks.size(); i++) {
        const Clock& c = g_clocks[i];
        auto mk = [&]() { WhenZone z; z.clock = c; z.name = DisplayName(c); return z; };
        consider(Lower(c.label), mk);
        if (!c.person.empty()) consider(Lower(c.person), mk);
    }
    for (auto& a : kAliases) {
        std::wstring city = a.city; city = city.substr(0, city.find(L','));
        consider(Lower(city), [&]() { WhenZone z; z.clock = ZoneClock(a.tz); z.clock.label = city; z.clock.lat = a.lat; z.clock.lon = a.lon; z.name = city; return z; });
    }
    for (auto& a : kAbbr)
        consider(a.key, [&]() { WhenZone z; z.clock = ZoneClock(a.tz); z.clock.label = a.name; z.name = a.name; return z; });
    // explicit offsets: utc+5:30, gmt-4, utc+0530
    for (const wchar_t* pre : { L"utc", L"gmt" }) {
        for (size_t p = t.find(pre); p != std::wstring::npos; p = t.find(pre, p + 1)) {
            size_t q = p + 3; if (q >= t.size() || (t[q] != L'+' && t[q] != L'-')) continue;
            int sign = t[q] == L'-' ? -1 : 1; q++;
            int h = 0, m = 0, digits = 0; size_t r = q;
            while (r < t.size() && iswdigit(t[r]) && digits < 4) { h = h * 10 + (t[r] - L'0'); r++; digits++; }
            if (digits == 0) continue;
            if (digits >= 3) { m = h % 100; h /= 100; }
            else if (r < t.size() && t[r] == L':' && r + 2 < t.size() + 1 && iswdigit(t[r + 1])) { m = _wtoi(t.substr(r + 1, 2).c_str()); r += 3; }
            if (h > 14 || m > 59) continue;
            WhenZone z; z.fixedOff = sign * (h * 60 + m); z.clock = ZoneClock(L"UTC");
            z.name = Fmt(L"UTC%lc%d%ls", sign < 0 ? L'−' : L'+', h, m ? Fmt(L":%02d", m).c_str() : L"");
            if (r - p >= bestLen) { bestLen = r - p; at = p; len = r - p; out = z; found = true; }
        }
    }
    return found;
}

static bool ParseClockTime(const std::wstring& t, int& hh, int& mm, size_t& at, size_t& len) {
    if (FindWord(t, L"noon") != std::wstring::npos) { hh = 12; mm = 0; at = FindWord(t, L"noon"); len = 4; return true; }
    if (FindWord(t, L"midnight") != std::wstring::npos) { hh = 0; mm = 0; at = FindWord(t, L"midnight"); len = 8; return true; }
    for (size_t i = 0; i < t.size(); i++) {
        if (!iswdigit(t[i]) || (i > 0 && (iswalnum(t[i - 1]) || t[i - 1] == L':' || t[i - 1] == L'/' || t[i - 1] == L'-' || t[i - 1] == L'.'))) continue;
        size_t j = i; int a = 0, nd = 0;
        while (j < t.size() && iswdigit(t[j]) && nd < 4) { a = a * 10 + (t[j] - L'0'); j++; nd++; }
        if (j < t.size() && iswdigit(t[j])) continue;   // 5+ digits: not a time
        int h = a, m = 0; bool colon = false, hmark = false;
        auto dig = [&](size_t q) { return q < t.size() && iswdigit(t[q]); };
        if (nd <= 2 && j < t.size() && (t[j] == L':' || t[j] == L'.') && dig(j + 1) && dig(j + 2) && !dig(j + 3)) {
            m = (t[j + 1] - L'0') * 10 + (t[j + 2] - L'0'); j += 3; colon = true;
        } else if (nd <= 2 && j < t.size() && t[j] == L'h' && (j + 1 >= t.size() || !iswalpha(t[j + 1]))) {
            hmark = true; j++;
            if (dig(j) && dig(j + 1)) { m = (t[j] - L'0') * 10 + (t[j + 1] - L'0'); j += 2; }
        } else if (nd >= 3) { h = a / 100; m = a % 100; }
        size_t k = j; while (k < t.size() && t[k] == L' ') k++;
        int ap = 0; size_t apLen = 0;
        if (t.compare(k, 2, L"am") == 0) { ap = 1; apLen = 2; } else if (t.compare(k, 2, L"pm") == 0) { ap = 2; apLen = 2; }
        else if (k < t.size() && (t[k] == L'a' || t[k] == L'p') && (k + 1 >= t.size() || !iswalpha(t[k + 1]))) { ap = t[k] == L'a' ? 1 : 2; apLen = 1; }
        if (ap && k + apLen < t.size() && iswalpha(t[k + apLen])) ap = 0;
        bool ok = colon || hmark || ap;
        if (nd >= 3 && !ap && !hmark) ok = false;   // "1530" alone is too ambiguous (could be a year or a number)
        if (!ok) continue;
        if (ap) { if (h < 1 || h > 12) continue; if (ap == 2 && h < 12) h += 12; if (ap == 1 && h == 12) h = 0; }
        if (h > 23 || m > 59) continue;
        hh = h; mm = m; at = i; len = (ap ? k + apLen : j) - i;
        return true;
    }
    return false;
}

static int MonthFromWord(const std::wstring& w) {
    static const wchar_t* names[] = { L"jan", L"feb", L"mar", L"apr", L"may", L"jun", L"jul", L"aug", L"sep", L"oct", L"nov", L"dec" };
    if (w.size() < 3) return 0;
    for (int i = 0; i < 12; i++) if (w.compare(0, 3, names[i]) == 0) return i + 1;
    return 0;
}
static int WeekdayFromWord(const std::wstring& w) {
    static const wchar_t* names[] = { L"sun", L"mon", L"tue", L"wed", L"thu", L"fri", L"sat" };
    if (w.size() < 3) return -1;
    for (int i = 0; i < 7; i++) if (w.compare(0, 3, names[i]) == 0) return i;
    return -1;
}

static std::wstring CleanWhenText(const std::wstring& text) {
    std::wstring t = Lower(text);
    if (t.size() > 400) t.resize(400);
    for (auto& c : t) if (c == L'\t' || c == L'\r' || c == L'\n' || c == L',' || c == L'(' || c == L')' || c == L'@' || c == L'?' || c == L'!' || c == L'"' || c == L'“' || c == L'”') c = L' ';
    auto replaceAll = [&](const std::wstring& a, const std::wstring& b) { for (size_t p = t.find(a); p != std::wstring::npos; p = t.find(a, p + b.size())) t.replace(p, a.size(), b); };
    replaceAll(L"a.m.", L"am"); replaceAll(L"p.m.", L"pm"); replaceAll(L"o'clock", L""); replaceAll(L"o’clock", L"");
    replaceAll(L"u.s.a.", L"usa"); replaceAll(L"u.s.", L"usa"); replaceAll(L"u.k.", L"uk");
    replaceAll(L"am-", L"am \u2013 "); replaceAll(L"pm-", L"pm \u2013 "); replaceAll(L"\u2013", L" \u2013 "); replaceAll(L"\u2014", L" \u2013 ");
    return t;
}
// t must already be cleaned. With `forced`, that zone is the source and the text's zone words must already be blanked out.
static bool ParseWhenT(const std::wstring& t, const WhenZone* forced, WhenResult& r, std::wstring* err, bool allowNow = false) {
    int hh = 0, mm = 0; size_t tAt = 0, tLen = 0;
    r.hasTime = ParseClockTime(t, hh, mm, tAt, tLen);
    if (!r.hasTime && !allowNow) { if (err) *err = L"no time found (try \"3pm\", \"15:30\" or \"9:30 am\")"; return false; }
    std::wstring rest = r.hasTime ? t.substr(0, tAt) + L" " + t.substr(tAt + tLen) : t;

    WhenZone z; size_t zAt = 0, zLen = 0;
    if (forced) { z = *forced; r.hasZone = true; }
    else {
        r.hasZone = FindWhenZone(rest, z, zAt, zLen);
        if (!r.hasZone) { z.clock = g_localClock; z.name = L"your time"; }
        else rest = rest.substr(0, zAt) + L" " + rest.substr(zAt + zLen);
    }
    r.zone = z;
    if (!r.hasTime) {   // "what time is it in Germany": now
        r.utc = NowMin(); return true;
    }

    // today in that zone
    FILETIME f; GetSystemTimeAsFileTime(&f);
    SYSTEMTIME zt;
    if (z.fixedOff != INT_MIN) zt = StOfMin((long long)(FtU(f) / TPM) + z.fixedOff);
    else zt = ClockAt(z.clock, FtU(f)).st;
    long long day = DaysOf(zt.wYear, zt.wMonth, zt.wDay);

    // date words
    std::vector<std::wstring> words; { std::wstring cur; for (wchar_t c : rest + L" ") { if (iswalnum(c) || c == L'-' || c == L'/') cur += c; else { if (!cur.empty()) words.push_back(cur); cur.clear(); } } }
    bool next = false;
    for (size_t i = 0; i < words.size(); i++) {
        const std::wstring& w = words[i];
        if (w == L"today" || w == L"tonight") { r.hasDate = true; continue; }
        if (w == L"tomorrow" || w == L"tmrw" || w == L"tmr" || w == L"tomorow") { day += 1; r.hasDate = true; continue; }
        if (w == L"yesterday") { day -= 1; r.hasDate = true; continue; }
        if (w == L"next") { next = true; continue; }
        int wd = WeekdayFromWord(w);
        if (wd >= 0 && (w.size() == 3 || w.size() == 4 || w.find(L"day") != std::wstring::npos || w == L"tues" || w == L"thur" || w == L"thurs")) {
            int ahead = (wd - WeekdayOf(day) + 7) % 7;
            if (next && ahead == 0) ahead = 7;
            day += ahead; r.hasDate = true; continue;
        }
        int mo = MonthFromWord(w);
        if (mo && w.size() >= 3 && (w.size() <= 4 || iswalpha(w[3]))) {
            int dd = 0, yy = zt.wYear;
            if (i + 1 < words.size() && iswdigit(words[i + 1][0])) { dd = _wtoi(words[i + 1].c_str()); if (i + 2 < words.size() && words[i + 2].size() == 4 && iswdigit(words[i + 2][0])) yy = _wtoi(words[i + 2].c_str()); }
            else if (i > 0 && iswdigit(words[i - 1][0])) { dd = _wtoi(words[i - 1].c_str()); if (i + 1 < words.size() && words[i + 1].size() == 4 && iswdigit(words[i + 1][0])) yy = _wtoi(words[i + 1].c_str()); }
            if (dd >= 1 && dd <= 31) {
                long long d2 = DaysOf(yy, mo, dd);
                if (d2) { if (d2 < day - 60 && yy == zt.wYear) d2 = DaysOf(yy + 1, mo, dd); day = d2; r.hasDate = true; }
            }
            continue;
        }
        int a, b, c;
        if (swscanf(w.c_str(), L"%d-%d-%d", &a, &b, &c) == 3 && a > 1900) { long long d2 = DaysOf(a, b, c); if (d2) { day = d2; r.hasDate = true; } continue; }
        if (swscanf(w.c_str(), L"%d/%d", &a, &b) == 2) {
            wchar_t fmt[80] = L""; GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE, fmt, 80);
            bool dayFirst = wcschr(fmt, L'd') && wcschr(fmt, L'M') && wcschr(fmt, L'd') < wcschr(fmt, L'M');
            int dd = dayFirst ? a : b, mo2 = dayFirst ? b : a;
            if (a > 12) { dd = a; mo2 = b; } else if (b > 12) { dd = b; mo2 = a; }
            int yy = zt.wYear; if (swscanf(w.c_str(), L"%*d/%*d/%d", &c) == 1) yy = c < 100 ? 2000 + c : c;
            long long d2 = DaysOf(yy, mo2, dd); if (d2) { day = d2; r.hasDate = true; }
        }
    }

    SYSTEMTIME lt = DateOf(day); lt.wHour = (WORD)hh; lt.wMinute = (WORD)mm;
    if (z.fixedOff != INT_MIN) r.utc = day * 1440 + hh * 60 + mm - z.fixedOff;
    else if (!LocalToUtcMin(z.clock, lt, r.utc)) { if (err) *err = L"couldn't convert that time"; return false; }
    return true;
}
struct CalcAnswer; static bool TimeCalc(const std::wstring& text, CalcAnswer& a);
static bool ParseWhenCalc(const std::wstring& text, WhenResult& r);   // defined with the calculator
static bool ParseWhen(std::wstring text, WhenResult& r, std::wstring* err = nullptr) {
    if (ParseWhenCalc(text, r)) return true;   // "9pm in india, what time in germany": India is the source
    return ParseWhenT(CleanWhenText(text), nullptr, r, err);
}

// "3:00 PM New York (Thu) = 12:30 AM Mumbai (Fri) = ..."
static std::wstring ConversionLine(const WhenResult& w) {
    auto at = [&](const Clock& c, int fixedOff) {
        SYSTEMTIME st = fixedOff != INT_MIN ? StOfMin(w.utc + fixedOff) : ClockAt(c, (ULONGLONG)w.utc * TPM).st;
        return FmtHMFull(st) + L" " + L"%NAME% (" + kDays[st.wDayOfWeek] + L")";
    };
    std::wstring src = at(w.zone.clock, w.zone.fixedOff);
    src.replace(src.find(L"%NAME%"), 6, w.zone.name);
    std::wstring s = src;
    SYSTEMTIME srcSt = w.zone.fixedOff != INT_MIN ? StOfMin(w.utc + w.zone.fixedOff) : ClockAt(w.zone.clock, (ULONGLONG)w.utc * TPM).st;
    for (size_t i = 0; i < g_clocks.size(); i++) {
        const Clock& c = g_clocks[i];
        CTime t = ClockAt(c, (ULONGLONG)w.utc * TPM);
        if (w.zone.fixedOff == INT_MIN && _wcsicmp(c.label.c_str(), w.zone.clock.label.c_str()) == 0 &&
            t.st.wHour == srcSt.wHour && t.st.wMinute == srcSt.wMinute) continue;
        std::wstring one = at(c, INT_MIN); one.replace(one.find(L"%NAME%"), 6, DisplayName(c));
        s += L" = " + one;
    }
    return s;
}

// ---------------------------------------------------------------- time calculator
// "9:00 pm in india, what time would it be in germany" · "3pm london to tokyo" · "10am usa → india tomorrow"
// The place next to the time is where the time is; every other place mentioned is a target.
// Countries with several zones (USA, Canada, Australia, Russia) answer for each main zone.
struct CountryZones { const wchar_t* keys; const wchar_t* name; const wchar_t* zones; };   // keys "|"-separated; zones "City=tz|City=tz"
static const CountryZones kCountryZones[] = {
    { L"india|bharat", L"India", L"India=India Standard Time" },
    { L"germany|deutschland", L"Germany", L"Berlin=W. Europe Standard Time" },
    { L"uk|united kingdom|britain|great britain|england|scotland|wales", L"UK", L"London=GMT Standard Time" },
    { L"ireland", L"Ireland", L"Dublin=GMT Standard Time" },
    { L"france", L"France", L"Paris=Romance Standard Time" },
    { L"spain", L"Spain", L"Madrid=Romance Standard Time" },
    { L"italy", L"Italy", L"Rome=W. Europe Standard Time" },
    { L"netherlands|holland", L"Netherlands", L"Amsterdam=W. Europe Standard Time" },
    { L"belgium", L"Belgium", L"Brussels=Romance Standard Time" },
    { L"switzerland", L"Switzerland", L"Zurich=W. Europe Standard Time" },
    { L"austria", L"Austria", L"Vienna=W. Europe Standard Time" },
    { L"sweden", L"Sweden", L"Stockholm=W. Europe Standard Time" },
    { L"norway", L"Norway", L"Oslo=W. Europe Standard Time" },
    { L"denmark", L"Denmark", L"Copenhagen=Romance Standard Time" },
    { L"finland", L"Finland", L"Helsinki=FLE Standard Time" },
    { L"poland", L"Poland", L"Warsaw=Central European Standard Time" },
    { L"czechia|czech republic", L"Czechia", L"Prague=Central Europe Standard Time" },
    { L"hungary", L"Hungary", L"Budapest=Central Europe Standard Time" },
    { L"portugal", L"Portugal", L"Lisbon=GMT Standard Time" },
    { L"greece", L"Greece", L"Athens=GTB Standard Time" },
    { L"romania", L"Romania", L"Bucharest=GTB Standard Time" },
    { L"ukraine", L"Ukraine", L"Kyiv=FLE Standard Time" },
    { L"turkey|türkiye|turkiye", L"Türkiye", L"Istanbul=Turkey Standard Time" },
    { L"russia", L"Russia", L"Moscow=Russian Standard Time|Yekaterinburg=Ekaterinburg Standard Time|Vladivostok=Vladivostok Standard Time" },
    { L"israel", L"Israel", L"Jerusalem=Israel Standard Time" },
    { L"egypt", L"Egypt", L"Cairo=Egypt Standard Time" },
    { L"saudi arabia|saudi|ksa", L"Saudi Arabia", L"Riyadh=Arab Standard Time" },
    { L"uae|united arab emirates|emirates", L"UAE", L"Dubai=Arabian Standard Time" },
    { L"qatar", L"Qatar", L"Doha=Arab Standard Time" },
    { L"kuwait", L"Kuwait", L"Kuwait City=Arab Standard Time" },
    { L"oman", L"Oman", L"Muscat=Arabian Standard Time" },
    { L"iran", L"Iran", L"Tehran=Iran Standard Time" },
    { L"iraq", L"Iraq", L"Baghdad=Arabic Standard Time" },
    { L"pakistan", L"Pakistan", L"Karachi=Pakistan Standard Time" },
    { L"bangladesh", L"Bangladesh", L"Dhaka=Bangladesh Standard Time" },
    { L"sri lanka", L"Sri Lanka", L"Colombo=Sri Lanka Standard Time" },
    { L"nepal", L"Nepal", L"Kathmandu=Nepal Standard Time" },
    { L"afghanistan", L"Afghanistan", L"Kabul=Afghanistan Standard Time" },
    { L"myanmar|burma", L"Myanmar", L"Yangon=Myanmar Standard Time" },
    { L"china|prc", L"China", L"Beijing=China Standard Time" },
    { L"hong kong", L"Hong Kong", L"Hong Kong=China Standard Time" },
    { L"taiwan", L"Taiwan", L"Taipei=Taipei Standard Time" },
    { L"japan", L"Japan", L"Tokyo=Tokyo Standard Time" },
    { L"south korea|korea", L"South Korea", L"Seoul=Korea Standard Time" },
    { L"singapore", L"Singapore", L"Singapore=Singapore Standard Time" },
    { L"malaysia", L"Malaysia", L"Kuala Lumpur=Singapore Standard Time" },
    { L"philippines", L"Philippines", L"Manila=Singapore Standard Time" },
    { L"thailand", L"Thailand", L"Bangkok=SE Asia Standard Time" },
    { L"vietnam|viet nam", L"Vietnam", L"Hanoi=SE Asia Standard Time" },
    { L"indonesia", L"Indonesia", L"Jakarta=SE Asia Standard Time" },
    { L"australia", L"Australia", L"Sydney=AUS Eastern Standard Time|Adelaide=Cen. Australia Standard Time|Perth=W. Australia Standard Time" },
    { L"new zealand|nz", L"New Zealand", L"Auckland=New Zealand Standard Time" },
    { L"usa|united states|united states of america|america|us|the states", L"USA", L"New York=Eastern Standard Time|Chicago=Central Standard Time|Denver=Mountain Standard Time|Los Angeles=Pacific Standard Time" },
    { L"canada", L"Canada", L"Toronto=Eastern Standard Time|Winnipeg=Central Standard Time|Calgary=Mountain Standard Time|Vancouver=Pacific Standard Time" },
    { L"mexico", L"Mexico", L"Mexico City=Central Standard Time (Mexico)" },
    { L"brazil", L"Brazil", L"São Paulo=E. South America Standard Time" },
    { L"argentina", L"Argentina", L"Buenos Aires=Argentina Standard Time" },
    { L"chile", L"Chile", L"Santiago=Pacific SA Standard Time" },
    { L"colombia", L"Colombia", L"Bogotá=SA Pacific Standard Time" },
    { L"peru", L"Peru", L"Lima=SA Pacific Standard Time" },
    { L"south africa", L"South Africa", L"Johannesburg=South Africa Standard Time" },
    { L"nigeria", L"Nigeria", L"Lagos=W. Central Africa Standard Time" },
    { L"kenya", L"Kenya", L"Nairobi=E. Africa Standard Time" },
    { L"ethiopia", L"Ethiopia", L"Addis Ababa=E. Africa Standard Time" },
    { L"morocco", L"Morocco", L"Casablanca=Morocco Standard Time" },
    { L"ghana", L"Ghana", L"Accra=Greenwich Standard Time" },
    { L"iceland", L"Iceland", L"Reykjavik=Greenwich Standard Time" },
};

struct PlaceHit { size_t at = 0, len = 0; std::wstring place; std::vector<WhenZone> zones; };
static std::vector<PlaceHit> FindAllPlaces(const std::wstring& t) {
    std::vector<PlaceHit> cand;
    auto wordBefore = [&](size_t p) {
        size_t e = p; while (e > 0 && t[e - 1] == L' ') e--;
        size_t b = e; while (b > 0 && iswalpha(t[b - 1])) b--;
        return t.substr(b, e - b);
    };
    auto addAll = [&](const std::wstring& key, auto make) {
        if (key.size() < 2) return;
        for (size_t p = FindWord(t, key); p != std::wstring::npos; p = FindWord(t, key, p + 1)) {
            if (key == L"us") { std::wstring w = wordBefore(p); if (w != L"in" && w != L"the" && w != L"to" && w != L"for") continue; }
            if (key == L"america") { std::wstring w = wordBefore(p); if (w == L"south" || w == L"latin" || w == L"central" || w == L"north") continue; }
            PlaceHit h; h.at = p; h.len = key.size(); make(h); cand.push_back(h);
        }
    };
    auto one = [](const Clock& c, const std::wstring& name) { WhenZone z; z.clock = c; z.name = name; return z; };
    for (size_t i = 0; i < g_clocks.size(); i++) {
        const Clock& c = g_clocks[i];
        auto mk = [&](PlaceHit& h) { h.place = DisplayName(c); h.zones = { one(c, DisplayName(c)) }; };
        addAll(Lower(c.label), mk);
        if (!c.person.empty()) addAll(Lower(c.person), mk);
    }
    for (auto& cz : kCountryZones) {
        std::wstring keys = cz.keys;
        for (size_t b = 0; b <= keys.size();) {
            size_t e = keys.find(L'|', b); if (e == std::wstring::npos) e = keys.size();
            addAll(keys.substr(b, e - b), [&](PlaceHit& h) {
                h.place = cz.name;
                std::wstring zs = cz.zones;
                for (size_t q = 0; q <= zs.size();) {
                    size_t f = zs.find(L'|', q); if (f == std::wstring::npos) f = zs.size();
                    std::wstring item = zs.substr(q, f - q); size_t eq = item.find(L'=');
                    Clock c = ZoneClock(item.substr(eq + 1)); c.label = item.substr(0, eq);
                    h.zones.push_back(one(c, c.label));
                    q = f + 1;
                }
                if (h.zones.size() == 1 && h.zones[0].name != cz.name) h.zones[0].name = std::wstring(cz.name) + L" (" + h.zones[0].name + L")";
                else if (h.zones.size() == 1) h.zones[0].name = cz.name;
            });
            b = e + 1;
        }
    }
    for (auto& a : kAliases) {
        std::wstring city = a.city; city = city.substr(0, city.find(L','));
        addAll(Lower(city), [&](PlaceHit& h) { Clock c = ZoneClock(a.tz); c.label = city; c.lat = a.lat; c.lon = a.lon; h.place = city; h.zones = { one(c, city) }; });
    }
    for (auto& a : kAbbr) {
        if (!wcscmp(a.key, L"z")) continue;
        addAll(a.key, [&](PlaceHit& h) {
            Clock c = a.tz == std::wstring(L"Local") ? g_localClock : ZoneClock(a.tz); c.label = a.name;
            std::wstring nm = a.tz == std::wstring(L"Local") ? std::wstring(L"your time") : std::wstring(a.name);
            bool isAbbr = wcslen(a.key) <= 4 && !wcschr(a.key, L' ') && wcscmp(a.key, L"uk") && wcscmp(a.key, L"ny") && wcscmp(a.key, L"la") && wcscmp(a.key, L"sf") && wcscmp(a.key, L"dc") && wcscmp(a.key, L"nyc");
            if (isAbbr && nm != L"your time") nm = Upper(a.key) + L" (" + nm + L")";
            h.place = nm; h.zones = { one(c, nm) };
        });
    }
    // fixed offsets: utc+5:30, gmt-4
    for (const wchar_t* pre : { L"utc", L"gmt" }) {
        for (size_t p = t.find(pre); p != std::wstring::npos; p = t.find(pre, p + 1)) {
            size_t q = p + 3; if (q >= t.size() || (t[q] != L'+' && t[q] != L'-')) continue;
            int sign = t[q] == L'-' ? -1 : 1; q++;
            int h = 0, m = 0, digits = 0; size_t r = q;
            while (r < t.size() && iswdigit(t[r]) && digits < 4) { h = h * 10 + (t[r] - L'0'); r++; digits++; }
            if (!digits) continue;
            if (digits >= 3) { m = h % 100; h /= 100; }
            else if (r + 2 < t.size() && t[r] == L':' && iswdigit(t[r + 1]) && iswdigit(t[r + 2])) { m = _wtoi(t.substr(r + 1, 2).c_str()); r += 3; }
            if (h > 14 || m > 59) continue;
            PlaceHit ph; ph.at = p; ph.len = r - p;
            WhenZone z; z.fixedOff = sign * (h * 60 + m); z.clock = ZoneClock(L"UTC");
            z.name = Fmt(L"UTC%lc%d%ls", sign < 0 ? L'−' : L'+', h, m ? Fmt(L":%02d", m).c_str() : L"");
            ph.place = z.name; ph.zones = { z }; cand.push_back(ph);
        }
    }
    // longest first; keep the ones that don't overlap
    std::stable_sort(cand.begin(), cand.end(), [](const PlaceHit& a, const PlaceHit& b) { return a.len > b.len; });
    std::vector<PlaceHit> out;
    for (auto& c : cand) {
        bool clash = false;
        for (auto& o : out) if (c.at < o.at + o.len && o.at < c.at + c.len) { clash = true; break; }
        if (!clash) out.push_back(c);
    }
    std::sort(out.begin(), out.end(), [](const PlaceHit& a, const PlaceHit& b) { return a.at < b.at; });
    return out;
}

struct CalcRow { std::wstring title, sub, copy; };
struct CalcAnswer {
    WhenResult when;                                    // the source moment (usable with ConvertTo)
    std::vector<PlaceHit> targets;                      // where to show it
    std::vector<CalcRow> rows;                          // one per target zone
    std::wstring copy;                                  // every row, one per line
};
static SYSTEMTIME ZoneTime(const WhenZone& z, long long utc) {
    return z.fixedOff != INT_MIN ? StOfMin(utc + z.fixedOff) : ClockAt(z.clock, (ULONGLONG)utc * TPM).st;
}
static int ZoneOffset(const WhenZone& z, long long utc) {
    SYSTEMTIME st = ZoneTime(z, utc);
    return (int)(DaysOf(st.wYear, st.wMonth, st.wDay) * 1440 + st.wHour * 60 + st.wMinute - utc);
}
static std::wstring DiffWords(int mins) {   // "4 h 30 min"
    mins = std::abs(mins);
    if (mins % 60 == 0) return Fmt(L"%d h", mins / 60);
    if (mins < 60) return Fmt(L"%d min", mins);
    return Fmt(L"%d h %d min", mins / 60, mins % 60);
}
static bool TimeCalc(const std::wstring& text, CalcAnswer& a) {
    std::wstring t = CleanWhenText(text);
    std::vector<PlaceHit> hits = FindAllPlaces(t);
    if (hits.empty()) return false;
    // blank the place words so they aren't read as dates ("mon", "sat") and the time parser skips them
    std::wstring blanked = t;
    for (auto& h : hits) for (size_t i = h.at; i < h.at + h.len && i < blanked.size(); i++) blanked[i] = L' ';
    int hh, mm; size_t tAt = 0, tLen = 0;
    bool hasTime = ParseClockTime(blanked, hh, mm, tAt, tLen);
    bool nowWords = !hasTime && (FindWord(t, L"now") != std::wstring::npos || t.find(L"time is it") != std::wstring::npos ||
                                 t.find(L"current time") != std::wstring::npos || t.find(L"time in") != std::wstring::npos || t.find(L"what time") != std::wstring::npos);
    if (!hasTime && !nowWords && hits.size() < 2) return false;

    // pick the source: the place nearest the time ("9pm in india", "india 9pm"); otherwise the first place
    int src = -1;
    if (hasTime) {
        size_t tEnd = tAt + tLen; long best = LONG_MAX;
        for (size_t i = 0; i < hits.size(); i++) {
            long gap = hits[i].at >= tEnd ? (long)(hits[i].at - tEnd) : (long)tAt - (long)(hits[i].at + hits[i].len);
            if (gap < 0) gap = 0;
            // a target word between the time and this place means it's where you want the answer, not the source
            std::wstring between = hits[i].at >= tEnd ? t.substr(tEnd, hits[i].at - tEnd) : L"";
            bool cue = false;
            for (const wchar_t* w : { L"to", L"into", L"what", L"convert", L"equals", L"=", L"->", L"→", L"for", L"would", L"will" })
                if ((wcslen(w) <= 2 && !iswalpha(w[0])) ? between.find(w) != std::wstring::npos : FindWord(between, w) != std::wstring::npos) cue = true;
            if (cue) gap += 10000;
            if (gap < best) { best = gap; src = (int)i; }
        }
        if (hits.size() == 1 && best >= 10000) src = -1;   // "9pm, what time is it in germany": from your time
    } else if (hits.size() >= 2 && !nowWords) src = 0;
    // with no time ("what time is it in germany") every place is a target, measured from now

    WhenZone srcZone;
    if (src >= 0) srcZone = hits[src].zones[0];
    else { srcZone.clock = g_localClock; srcZone.name = L"your time"; }
    if (hasTime) {
        if (!ParseWhenT(blanked, &srcZone, a.when, nullptr)) return false;
    } else { a.when = WhenResult{}; a.when.zone = srcZone; a.when.utc = NowMin() + g_shift; a.when.hasZone = true; }
    for (size_t i = 0; i < hits.size(); i++) if ((int)i != src) a.targets.push_back(hits[i]);
    if (a.targets.empty()) return false;   // only one place: the normal "3pm tokyo" conversion covers it

    long long u = a.when.utc;
    SYSTEMTIME s0 = ZoneTime(srcZone, u);
    long long d0 = DaysOf(s0.wYear, s0.wMonth, s0.wDay);
    std::wstring srcName = src >= 0 ? hits[src].place : std::wstring(L"your time");
    std::wstring when = hasTime ? L"when it's " + FmtHMFull(s0) + L" " + kDays[s0.wDayOfWeek] + (src >= 0 ? L" in " + srcName : std::wstring(L" for you")) : std::wstring(L"right now");
    std::wstring all;
    for (auto& tg : a.targets) {
        for (size_t k = 0; k < tg.zones.size(); k++) {
            const WhenZone& z = tg.zones[k];
            SYSTEMTIME st = ZoneTime(z, u);
            long long dd = DaysOf(st.wYear, st.wMonth, st.wDay) - d0;
            std::wstring nm = tg.zones.size() > 1 ? tg.place + L" · " + z.name : tg.place;
            std::wstring dayNote = hasTime && dd > 0 ? L" (next day)" : hasTime && dd < 0 ? L" (day before)" : L"";
            int diff = ZoneOffset(z, u) - ZoneOffset(srcZone, u);
            std::wstring vs = src >= 0 ? srcName : std::wstring(L"you");
            std::wstring rel = diff == 0 ? L"same time as " + vs : DiffWords(diff) + (diff > 0 ? L" ahead of " : L" behind ") + vs;
            CalcRow r;
            r.title = FmtHMFull(st) + L" " + kDays[st.wDayOfWeek] + dayNote + L"  ·  " + nm;
            r.sub = when + L"  ·  " + rel;
            r.copy = (hasTime ? FmtHMFull(s0) + L" " + kDays[s0.wDayOfWeek] + L" " + srcName + L" = " : std::wstring(L"Now: ")) +
                     FmtHMFull(st) + L" " + kDays[st.wDayOfWeek] + L" " + nm;
            all += (all.empty() ? L"" : L"\r\n") + r.copy;
            a.rows.push_back(r);
        }
    }
    a.copy = all;
    return true;
}
static bool ParseWhenCalc(const std::wstring& text, WhenResult& r) {
    CalcAnswer a;
    if (!TimeCalc(text, a) || !a.when.hasTime) return false;
    r = a.when; return true;
}

// ---------------------------------------------------------------- calculator: durations, "until", "between", ranges
// "9pm IST + 3h 45m" · "now + 90 min" · "how long until 6pm London" · "hours between 9am NY and 5pm Tokyo"
// "9am-5pm India in Germany" (a window, with the overlap with Germany's working hours)
static bool UnitAt(const std::wstring& t, size_t& j, int& perMin) {
    static const std::pair<const wchar_t*, int> units[] = {
        { L"hours", 60 }, { L"hour", 60 }, { L"hrs", 60 }, { L"hr", 60 }, { L"h", 60 },
        { L"minutes", 1 }, { L"minute", 1 }, { L"mins", 1 }, { L"min", 1 }, { L"m", 1 },
        { L"days", 1440 }, { L"day", 1440 }, { L"d", 1440 } };
    size_t k = j; while (k < t.size() && t[k] == L' ') k++;
    for (auto& u : units) {
        size_t n = wcslen(u.first);
        if (t.compare(k, n, u.first) == 0 && (k + n >= t.size() || !iswalpha(t[k + n]))) { j = k + n; perMin = u.second; return true; }
    }
    return false;
}
// parses "3h 45m", "3h45", "90 min", "1.5 hours", "1:30" (after + / -) starting at i; returns minutes and the end
static bool DurationAt(const std::wstring& t, size_t i, bool colonOk, int& mins, size_t& end) {
    size_t j = i; while (j < t.size() && t[j] == L' ') j++;
    if (colonOk && j + 3 < t.size() + 1 && iswdigit(t[j])) {   // "1:30"
        size_t k = j; int h = 0; while (k < t.size() && iswdigit(t[k]) && k - j < 3) h = h * 10 + (t[k++] - L'0');
        if (k + 2 < t.size() + 1 && k < t.size() && t[k] == L':' && k + 2 < t.size() + 1 && iswdigit(t[k + 1]) && iswdigit(t[k + 2]) && (k + 3 >= t.size() || !iswalnum(t[k + 3]))) {
            mins = h * 60 + (t[k + 1] - L'0') * 10 + (t[k + 2] - L'0'); end = k + 3; return mins > 0;
        }
    }
    double total = 0; int parts = 0;
    for (;;) {
        size_t k = j; while (k < t.size() && t[k] == L' ') k++;
        if (k >= t.size() || !iswdigit(t[k])) break;
        size_t q = k; double v = 0; while (q < t.size() && iswdigit(t[q])) v = v * 10 + (t[q++] - L'0');
        if (q + 1 < t.size() && t[q] == L'.' && iswdigit(t[q + 1])) { double f = 0.1; q++; while (q < t.size() && iswdigit(t[q])) { v += (t[q++] - L'0') * f; f /= 10; } }
        int per = 0; size_t u = q;
        if (!UnitAt(t, u, per)) {
            if (parts == 1 && per == 0) {   // "3h45": minutes without a unit after an hour part
                if (q - k <= 2 && v < 60 && (q >= t.size() || !iswalpha(t[q]))) { total += v; j = q; parts++; }
            }
            break;
        }
        // "5pm" is a time, not 5 minutes: units must not be followed by more letters (handled in UnitAt), and "m" right after
        // digits followed by nothing else is fine. Reject "am"/"pm" (UnitAt never matches those).
        total += v * per; j = u; parts++;
        if (parts >= 3) break;
    }
    if (!parts || total <= 0 || total > 60 * 24 * 60) return false;
    mins = (int)llround(total); end = j; return true;
}
// finds a duration term, blanks it in t and returns signed minutes
static bool TakeDuration(std::wstring& t, int& signedMins) {
    auto blank = [&](size_t a, size_t b) { for (size_t i = a; i < b && i < t.size(); i++) t[i] = L' '; };
    // "+ 3h", "- 90 min", "plus 2 hours", "minus 30m", "add 2h"
    for (size_t i = 0; i < t.size(); i++) {
        int sign = 0; size_t after = i;
        if (t[i] == L'+') { sign = 1; after = i + 1; }
        else if (t[i] == L'-' && (i == 0 || t[i - 1] == L' ') && i + 1 < t.size() && (t[i + 1] == L' ' || iswdigit(t[i + 1]))) { sign = -1; after = i + 1; }
        else {
            for (auto w : { std::make_pair(L"plus", 1), std::make_pair(L"add", 1), std::make_pair(L"minus", -1), std::make_pair(L"subtract", -1) }) {
                size_t n = wcslen(w.first);
                if (t.compare(i, n, w.first) == 0 && WordAt(t, i, n)) { sign = w.second; after = i + n; break; }
            }
        }
        if (!sign) continue;
        int m; size_t e;
        if (DurationAt(t, after, true, m, e)) { signedMins = sign * m; blank(i, e); return true; }
    }
    // "3 hours later / after / from now", "2h before / earlier / ago", "in 3 hours"
    for (size_t i = 0; i < t.size(); i++) {
        if (!iswdigit(t[i]) || (i > 0 && (iswalnum(t[i - 1]) || t[i - 1] == L':'))) continue;
        int m; size_t e;
        if (!DurationAt(t, i, false, m, e)) continue;
        size_t k = e; while (k < t.size() && t[k] == L' ') k++;
        auto word = [&](const wchar_t* w) { size_t n = wcslen(w); return t.compare(k, n, w) == 0 && WordAt(t, k, n) ? n : 0; };
        size_t n;
        if ((n = word(L"later")) || (n = word(L"after")) || (n = word(L"from now")) || (n = word(L"from"))) { signedMins = m; blank(i, k + n); return true; }
        if ((n = word(L"before")) || (n = word(L"earlier")) || (n = word(L"ago"))) { signedMins = -m; blank(i, k + n); return true; }
        // "in 3 hours" (only when "in" sits right before and no clock time follows)
        size_t b = i; while (b > 0 && t[b - 1] == L' ') b--;
        if (b >= 2 && t.compare(b - 2, 2, L"in") == 0 && (b == 2 || t[b - 3] == L' ')) { signedMins = m; blank(b - 2, e); return true; }
    }
    return false;
}
struct TimeSpot { size_t at, len; int hh, mm; };
static std::vector<TimeSpot> FindTimes(std::wstring t, int maxN = 3) {
    std::vector<TimeSpot> out;
    for (int k = 0; k < maxN; k++) {
        int hh, mm; size_t at, len;
        if (!ParseClockTime(t, hh, mm, at, len)) break;
        out.push_back({ at, len, hh, mm });
        for (size_t i = at; i < at + len; i++) t[i] = L' ';
        if (FindWord(t, L"noon") == std::wstring::npos && FindWord(t, L"midnight") == std::wstring::npos) {}
    }
    std::sort(out.begin(), out.end(), [](const TimeSpot& a, const TimeSpot& b) { return a.at < b.at; });
    return out;
}
// the moment for one time spot, read in zone z, with any date words in the text
static bool MomentFor(const std::wstring& blanked, const TimeSpot& ts, const std::vector<TimeSpot>& all, const WhenZone& z, long long& utc) {
    std::wstring only = blanked;
    for (auto& o : all) if (o.at != ts.at) for (size_t i = o.at; i < o.at + o.len; i++) only[i] = L' ';
    WhenResult r;
    if (!ParseWhenT(only, &z, r, nullptr)) return false;
    utc = r.utc; return true;
}
static std::wstring DurWords(long long m) {
    bool neg = m < 0; if (neg) m = -m;
    std::wstring s;
    if (m >= 1440) s = Fmt(L"%lld d ", m / 1440);
    long long r = m % 1440;
    if (r >= 60 || m < 60) s += Fmt(L"%lld h", r / 60);
    if (r % 60) s += Fmt(L"%ls%lld min", (r >= 60) ? L" " : L"", r % 60);
    if (r == 0 && m >= 1440) s.pop_back();
    if (m < 60) s = Fmt(L"%lld min", m);
    return (neg ? L"−" : L"") + s;
}
static std::wstring WhenIn(const WhenZone& z, long long u, const std::wstring& name, long long refDay = LLONG_MIN) {
    SYSTEMTIME st = ZoneTime(z, u);
    std::wstring d = kDays[st.wDayOfWeek];
    if (refDay != LLONG_MIN) { long long dd = DaysOf(st.wYear, st.wMonth, st.wDay) - refDay; if (dd > 0) d += L", next day"; else if (dd < 0) d += L", day before"; }
    return FmtHMFull(st) + L" " + d + L" · " + name;
}
static long long DayOfZone(const WhenZone& z, long long u) { SYSTEMTIME st = ZoneTime(z, u); return DaysOf(st.wYear, st.wMonth, st.wDay); }
// working-hours window of a zone (clock's own hours when it is one of yours), intersected with [a, b)
static long long WorkOverlap(const WhenZone& z, long long a, long long b, long long& o1, long long& o2) {
    int ws = S.workStart, we = S.workEnd; int offMask = 0x41;
    for (auto& c : g_clocks) if (c.tzid == z.clock.tzid && c.label == z.clock.label) { ws = WorkStartOf(c); we = WorkEndOf(c); offMask = c.offMask; }
    long long best = 0; o1 = o2 = -1;
    for (long long d = DayOfZone(z, a) - 1; d <= DayOfZone(z, b) + 1; d++) {
        SYSTEMTIME day = DateOf(d);
        if (offMask & (1 << WeekdayOf(d))) continue;
        SYSTEMTIME s1 = day; s1.wHour = (WORD)ws; SYSTEMTIME s2 = day; s2.wHour = (WORD)(we % 24);
        long long w1, w2;
        if (z.fixedOff != INT_MIN) { w1 = d * 1440 + ws * 60 - z.fixedOff; w2 = d * 1440 + we * 60 - z.fixedOff; }
        else { if (!LocalToUtcMin(z.clock, s1, w1) || !LocalToUtcMin(z.clock, s2, w2)) continue; if (we >= 24 || w2 <= w1) w2 += 1440; }
        long long x1 = std::max(a, w1), x2 = std::min(b, w2);
        if (x2 - x1 > best) { best = x2 - x1; o1 = x1; o2 = x2; }
    }
    return best;
}
static void AddPlainRow(CalcAnswer& a, const std::wstring& title, const std::wstring& sub) { CalcRow r; r.title = title; r.sub = sub; r.copy = title + L" (" + sub + L")"; a.rows.push_back(r); a.copy += (a.copy.empty() ? L"" : L"\r\n") + r.copy; }

static bool CalcQuery(const std::wstring& text, CalcAnswer& a) {
    a = CalcAnswer{};
    std::wstring t = CleanWhenText(text);
    int durMins = 0; bool hasDur = TakeDuration(t, durMins);
    std::vector<PlaceHit> hits = FindAllPlaces(t);
    std::wstring blanked = t;
    for (auto& h : hits) for (size_t i = h.at; i < h.at + h.len && i < blanked.size(); i++) blanked[i] = L' ';
    std::vector<TimeSpot> times = FindTimes(blanked);
    bool wantUntil = FindWord(t, L"until") != std::wstring::npos || FindWord(t, L"till") != std::wstring::npos || FindWord(t, L"untill") != std::wstring::npos ||
                     t.find(L"how long") != std::wstring::npos || t.find(L"time left") != std::wstring::npos || t.find(L"how much time") != std::wstring::npos;
    bool wantBetween = FindWord(t, L"between") != std::wstring::npos || t.find(L"how many hours") != std::wstring::npos || t.find(L"hours from") != std::wstring::npos ||
                       FindWord(t, L"duration") != std::wstring::npos || FindWord(t, L"difference") != std::wstring::npos || (wantUntil && times.size() == 2);
    long long now = NowMin();
    auto localZone = []() { WhenZone z; z.clock = g_localClock; z.name = L"your time"; return z; };
    auto nearest = [&](const TimeSpot& ts, int exclude) {
        int best = -1; long bestGap = LONG_MAX;
        for (int i = 0; i < (int)hits.size(); i++) {
            if (i == exclude) continue;
            long gap = hits[i].at >= ts.at + ts.len ? (long)(hits[i].at - ts.at - ts.len) : (long)ts.at - (long)(hits[i].at + hits[i].len);
            if (gap < 0) gap = 0;
            std::wstring between = hits[i].at >= ts.at + ts.len ? t.substr(ts.at + ts.len, hits[i].at - ts.at - ts.len) : L"";
            for (const wchar_t* w : { L"to", L"into", L"what", L"convert", L"for", L"would", L"will", L"in my" }) if (FindWord(between, w) != std::wstring::npos) gap += 10000;
            if (gap < bestGap) { bestGap = gap; best = i; }
        }
        return best;
    };

    // 1) between two times: a duration
    if (times.size() == 2 && wantBetween) {
        int p1 = -1, p2 = -1;
        if (hits.size() == 2) { p1 = hits[0].at < hits[1].at ? 0 : 1; p2 = 1 - p1; if (!(hits[p1].at < times[1].at)) { p1 = nearest(times[0], -1); p2 = nearest(times[1], p1); } }
        else if (hits.size() == 1) { p1 = nearest(times[0], -1); p2 = p1; }
        WhenZone z1 = p1 >= 0 ? hits[p1].zones[0] : localZone(), z2 = p2 >= 0 ? hits[p2].zones[0] : localZone();
        std::wstring n1 = p1 >= 0 ? hits[p1].place : L"your time", n2 = p2 >= 0 ? hits[p2].place : L"your time";
        long long u1, u2;
        if (!MomentFor(blanked, times[0], times, z1, u1) || !MomentFor(blanked, times[1], times, z2, u2)) return false;
        bool dated = false; { WhenResult r; std::wstring only = blanked; for (auto& o : times) for (size_t i = o.at; i < o.at + o.len; i++) only[i] = L' ';
                              ParseWhenT(only, &z1, r, nullptr, true); dated = r.hasDate; }
        if (!dated) while (u2 < u1) u2 += 1440;
        a.when.utc = u1; a.when.zone = z1; a.when.hasTime = a.when.hasZone = true;
        AddPlainRow(a, DurWords(u2 - u1), L"from " + WhenIn(z1, u1, n1) + L" to " + WhenIn(z2, u2, n2, DayOfZone(z1, u1)));
        return true;
    }
    // 2) how long until a time
    if (times.size() == 1 && wantUntil && !hasDur) {
        int p = hits.empty() ? -1 : nearest(times[0], -1);
        WhenZone z = p >= 0 ? hits[p].zones[0] : localZone(); std::wstring nm = p >= 0 ? hits[p].place : L"your time";
        long long u; if (!MomentFor(blanked, times[0], times, z, u)) return false;
        WhenResult r0; { std::wstring only = blanked; ParseWhenT(only, &z, r0, nullptr); }
        if (!r0.hasDate) while (u <= now) u += 1440;
        a.when.utc = u; a.when.zone = z; a.when.hasTime = a.when.hasZone = true;
        std::wstring sub = WhenIn(z, u, nm, DayOfZone(z, now));
        if (p >= 0) { WhenZone lz = localZone(); sub += L"  =  " + WhenIn(lz, u, L"your time", DayOfZone(lz, now)); }
        AddPlainRow(a, DurWords(u - now) + L" to go", L"until " + sub);
        for (int i = 0; i < (int)hits.size(); i++) if (i != p) for (auto& z2 : hits[i].zones) AddPlainRow(a, WhenIn(z2, u, hits[i].zones.size() > 1 ? hits[i].place + L" · " + z2.name : hits[i].place), L"the same moment");
        return true;
    }
    // 3) a window: "9am-5pm India in Germany"
    if (times.size() == 2 && !hasDur) {
        std::wstring mid = blanked.substr(times[0].at + times[0].len, times[1].at - times[0].at - times[0].len);
        std::wstring m2 = mid; m2.erase(std::remove(m2.begin(), m2.end(), L' '), m2.end());
        bool joined = m2 == L"–" || m2 == L"-" || m2 == L"to" || m2 == L"till" || m2 == L"until" || m2 == L"and";
        if (joined) {
            TimeSpot span{ times[0].at, times[1].at + times[1].len - times[0].at, 0, 0 };
            int p = hits.empty() ? -1 : nearest(span, -1);
            WhenZone z = p >= 0 ? hits[p].zones[0] : localZone(); std::wstring nm = p >= 0 ? hits[p].place : L"your time";
            long long u1, u2;
            if (!MomentFor(blanked, times[0], times, z, u1) || !MomentFor(blanked, times[1], times, z, u2)) return false;
            if (u2 <= u1) u2 += 1440;
            a.when.utc = u1; a.when.zone = z; a.when.hasTime = a.when.hasZone = true;
            std::vector<std::pair<std::wstring, WhenZone>> tg;
            for (int i = 0; i < (int)hits.size(); i++) if (i != p) for (auto& z2 : hits[i].zones) tg.push_back({ hits[i].zones.size() > 1 ? hits[i].place + L" · " + z2.name : hits[i].place, z2 });
            if (tg.empty()) for (size_t i = 0; i < g_clocks.size(); i++) { WhenZone z2; z2.clock = g_clocks[i]; z2.name = DisplayName((int)i); if (g_clocks[i].kind != 1) tg.push_back({ z2.name, z2 }); }
            long long d0 = DayOfZone(z, u1);
            std::wstring srcTxt = FmtHMFull(ZoneTime(z, u1)) + L"–" + FmtHMFull(ZoneTime(z, u2)) + L" " + kDays[ZoneTime(z, u1).wDayOfWeek] + L" in " + nm;
            for (auto& x : tg) {
                SYSTEMTIME s1 = ZoneTime(x.second, u1), s2 = ZoneTime(x.second, u2);
                long long dd = DaysOf(s1.wYear, s1.wMonth, s1.wDay) - d0;
                std::wstring title = FmtHMFull(s1) + L" – " + FmtHMFull(s2) + L" " + kDays[s1.wDayOfWeek] + (dd > 0 ? L" (next day)" : dd < 0 ? L" (day before)" : L"") + L"  ·  " + x.first;
                long long o1, o2; long long ov = WorkOverlap(x.second, u1, u2, o1, o2);
                std::wstring sub = srcTxt + L"  ·  " + (ov > 0 ? L"overlaps their working hours " + FmtHMFull(ZoneTime(x.second, o1)) + L"–" + FmtHMFull(ZoneTime(x.second, o2)) + L" (" + DurWords(ov) + L")"
                                                       : std::wstring(L"outside their working hours"));
                CalcRow r; r.title = title; r.sub = sub; r.copy = title + L" (" + srcTxt + L")"; a.rows.push_back(r);
                a.copy += (a.copy.empty() ? L"" : L"\r\n") + r.copy;
                PlaceHit ph; ph.place = x.first; ph.zones = { x.second }; a.targets.push_back(ph);
            }
            return !a.rows.empty();
        }
    }
    // 4) arithmetic: "9pm IST + 3h 45m", "now + 90 min", "in 3 hours"
    if (hasDur) {
        WhenZone z = localZone(); std::wstring nm = L"your time"; int p = -1;
        long long base;
        if (!times.empty()) {
            p = hits.empty() ? -1 : nearest(times[0], -1);
            if (p >= 0) { z = hits[p].zones[0]; nm = hits[p].place; }
            if (!MomentFor(blanked, times[0], times, z, base)) return false;
        } else {
            base = now + g_shift;
            // "now + 3h in tokyo": the first place is where you want the answer
        }
        long long u = base + durMins;
        a.when.utc = u; a.when.zone = z; a.when.hasTime = a.when.hasZone = true;
        std::wstring how = (times.empty() ? std::wstring(L"now") : FmtHMFull(ZoneTime(z, base)) + L" " + kDays[ZoneTime(z, base).wDayOfWeek]) +
                           (durMins >= 0 ? L" + " : L" − ") + DurWords(std::abs(durMins));
        long long d0 = DayOfZone(z, base);
        if (times.empty() && !hits.empty()) {
            for (auto& h : hits) for (auto& z2 : h.zones) AddPlainRow(a, WhenIn(z2, u, h.zones.size() > 1 ? h.place + L" · " + z2.name : h.place, DayOfZone(z2, base)), how + L" (" + h.place + L")");
        } else {
            AddPlainRow(a, WhenIn(z, u, nm, d0), how + (p >= 0 ? L" in " + nm : L""));
            for (int i = 0; i < (int)hits.size(); i++) if (i != p) for (auto& z2 : hits[i].zones) AddPlainRow(a, WhenIn(z2, u, hits[i].zones.size() > 1 ? hits[i].place + L" · " + z2.name : hits[i].place, d0), L"the same moment");
        }
        return true;
    }
    // 5) plain conversion between places
    return TimeCalc(text, a);
}
