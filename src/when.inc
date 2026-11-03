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
