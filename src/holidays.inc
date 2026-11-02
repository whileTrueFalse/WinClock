// ---------------------------------------------------------------- public holidays (offline)
// Rules for fixed dates, "nth weekday" dates and Easter-relative dates, plus a small table of
// lunar / announced dates. Lunar Islamic dates depend on moon sighting and can move by a day.
// Add or override dates in WinClock.ini, section [Holidays], e.g.   IN=2026-11-09 Diwali (observed)

struct Country { const wchar_t* cc; const wchar_t* name; };
static const Country kCountries[] = {
    { L"US", L"United States" }, { L"CA", L"Canada" }, { L"MX", L"Mexico" }, { L"BR", L"Brazil" },
    { L"GB", L"United Kingdom" }, { L"IE", L"Ireland" }, { L"DE", L"Germany" }, { L"FR", L"France" },
    { L"ES", L"Spain" }, { L"NL", L"Netherlands" }, { L"IN", L"India" }, { L"AE", L"United Arab Emirates" },
    { L"SG", L"Singapore" }, { L"CN", L"China" }, { L"JP", L"Japan" }, { L"AU", L"Australia" }, { L"NZ", L"New Zealand" },
};
static const wchar_t* CountryName(const std::wstring& cc) {
    for (auto& c : kCountries) if (_wcsicmp(c.cc, cc.c_str()) == 0) return c.name;
    return L"";
}

// kind: 0 fixed (m/d), 1 nth weekday (m, wd 0=Sun, n 1..5 or -1 = last), 2 Easter + offset, 3 weekday on/before (m/d)
// obs:  0 none, 1 US style (Sat->Fri, Sun->Mon), 2 UK style (Sat/Sun -> next Mon), 3 Sun -> Mon
struct HolRule { const wchar_t* cc; int kind; int m, d, wd, n, easter, obs; const wchar_t* name; };
static const HolRule kHolRules[] = {
    // United States (federal)
    { L"US", 0, 1, 1, 0, 0, 0, 1, L"New Year's Day" }, { L"US", 1, 1, 0, 1, 3, 0, 0, L"Martin Luther King Jr. Day" },
    { L"US", 1, 2, 0, 1, 3, 0, 0, L"Presidents' Day" }, { L"US", 1, 5, 0, 1, -1, 0, 0, L"Memorial Day" },
    { L"US", 0, 6, 19, 0, 0, 0, 1, L"Juneteenth" }, { L"US", 0, 7, 4, 0, 0, 0, 1, L"Independence Day" },
    { L"US", 1, 9, 0, 1, 1, 0, 0, L"Labor Day" }, { L"US", 1, 10, 0, 1, 2, 0, 0, L"Columbus Day" },
    { L"US", 0, 11, 11, 0, 0, 0, 1, L"Veterans Day" }, { L"US", 1, 11, 0, 4, 4, 0, 0, L"Thanksgiving" },
    { L"US", 0, 12, 25, 0, 0, 0, 1, L"Christmas Day" },
    // Canada (federal)
    { L"CA", 0, 1, 1, 0, 0, 0, 3, L"New Year's Day" }, { L"CA", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"CA", 3, 5, 24, 1, 0, 0, 0, L"Victoria Day" }, { L"CA", 0, 7, 1, 0, 0, 0, 3, L"Canada Day" },
    { L"CA", 1, 9, 0, 1, 1, 0, 0, L"Labour Day" }, { L"CA", 0, 9, 30, 0, 0, 0, 3, L"Truth and Reconciliation Day" },
    { L"CA", 1, 10, 0, 1, 2, 0, 0, L"Thanksgiving" }, { L"CA", 0, 11, 11, 0, 0, 0, 3, L"Remembrance Day" },
    { L"CA", 0, 12, 25, 0, 0, 0, 2, L"Christmas Day" }, { L"CA", 0, 12, 26, 0, 0, 0, 2, L"Boxing Day" },
    // Mexico
    { L"MX", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"MX", 1, 2, 0, 1, 1, 0, 0, L"Constitution Day" },
    { L"MX", 1, 3, 0, 1, 3, 0, 0, L"Benito Juárez's Birthday" }, { L"MX", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" },
    { L"MX", 0, 9, 16, 0, 0, 0, 0, L"Independence Day" }, { L"MX", 1, 11, 0, 1, 3, 0, 0, L"Revolution Day" },
    { L"MX", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    // Brazil (national)
    { L"BR", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"BR", 2, 0, 0, 0, 0, -48, 0, L"Carnival" },
    { L"BR", 2, 0, 0, 0, 0, -47, 0, L"Carnival" }, { L"BR", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"BR", 0, 4, 21, 0, 0, 0, 0, L"Tiradentes" }, { L"BR", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" },
    { L"BR", 2, 0, 0, 0, 0, 60, 0, L"Corpus Christi" }, { L"BR", 0, 9, 7, 0, 0, 0, 0, L"Independence Day" },
    { L"BR", 0, 10, 12, 0, 0, 0, 0, L"Our Lady of Aparecida" }, { L"BR", 0, 11, 2, 0, 0, 0, 0, L"All Souls' Day" },
    { L"BR", 0, 11, 15, 0, 0, 0, 0, L"Republic Day" }, { L"BR", 0, 11, 20, 0, 0, 0, 0, L"Black Consciousness Day" },
    { L"BR", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    // United Kingdom (England & Wales bank holidays)
    { L"GB", 0, 1, 1, 0, 0, 0, 2, L"New Year's Day" }, { L"GB", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"GB", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" }, { L"GB", 1, 5, 0, 1, 1, 0, 0, L"Early May bank holiday" },
    { L"GB", 1, 5, 0, 1, -1, 0, 0, L"Spring bank holiday" }, { L"GB", 1, 8, 0, 1, -1, 0, 0, L"Summer bank holiday" },
    { L"GB", 0, 12, 25, 0, 0, 0, 2, L"Christmas Day" }, { L"GB", 0, 12, 26, 0, 0, 0, 2, L"Boxing Day" },
    // Ireland
    { L"IE", 0, 1, 1, 0, 0, 0, 2, L"New Year's Day" }, { L"IE", 0, 3, 17, 0, 0, 0, 2, L"St Patrick's Day" },
    { L"IE", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" }, { L"IE", 1, 5, 0, 1, 1, 0, 0, L"May Day" },
    { L"IE", 1, 6, 0, 1, 1, 0, 0, L"June bank holiday" }, { L"IE", 1, 8, 0, 1, 1, 0, 0, L"August bank holiday" },
    { L"IE", 1, 10, 0, 1, -1, 0, 0, L"October bank holiday" }, { L"IE", 0, 12, 25, 0, 0, 0, 2, L"Christmas Day" },
    { L"IE", 0, 12, 26, 0, 0, 0, 2, L"St Stephen's Day" },
    // Germany (nationwide)
    { L"DE", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"DE", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"DE", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" }, { L"DE", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" },
    { L"DE", 2, 0, 0, 0, 0, 39, 0, L"Ascension Day" }, { L"DE", 2, 0, 0, 0, 0, 50, 0, L"Whit Monday" },
    { L"DE", 0, 10, 3, 0, 0, 0, 0, L"German Unity Day" }, { L"DE", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    { L"DE", 0, 12, 26, 0, 0, 0, 0, L"St Stephen's Day" },
    // France
    { L"FR", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"FR", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" },
    { L"FR", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" }, { L"FR", 0, 5, 8, 0, 0, 0, 0, L"Victory in Europe Day" },
    { L"FR", 2, 0, 0, 0, 0, 39, 0, L"Ascension Day" }, { L"FR", 2, 0, 0, 0, 0, 50, 0, L"Whit Monday" },
    { L"FR", 0, 7, 14, 0, 0, 0, 0, L"Bastille Day" }, { L"FR", 0, 8, 15, 0, 0, 0, 0, L"Assumption" },
    { L"FR", 0, 11, 1, 0, 0, 0, 0, L"All Saints' Day" }, { L"FR", 0, 11, 11, 0, 0, 0, 0, L"Armistice Day" },
    { L"FR", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    // Spain (national)
    { L"ES", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"ES", 0, 1, 6, 0, 0, 0, 0, L"Epiphany" },
    { L"ES", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" }, { L"ES", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" },
    { L"ES", 0, 8, 15, 0, 0, 0, 0, L"Assumption" }, { L"ES", 0, 10, 12, 0, 0, 0, 0, L"National Day" },
    { L"ES", 0, 11, 1, 0, 0, 0, 0, L"All Saints' Day" }, { L"ES", 0, 12, 6, 0, 0, 0, 0, L"Constitution Day" },
    { L"ES", 0, 12, 8, 0, 0, 0, 0, L"Immaculate Conception" }, { L"ES", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    // Netherlands
    { L"NL", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"NL", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" },
    { L"NL", 2, 0, 0, 0, 0, 39, 0, L"Ascension Day" }, { L"NL", 2, 0, 0, 0, 0, 50, 0, L"Whit Monday" },
    { L"NL", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" }, { L"NL", 0, 12, 26, 0, 0, 0, 0, L"Boxing Day" },
    // India (national gazetted)
    { L"IN", 0, 1, 26, 0, 0, 0, 0, L"Republic Day" }, { L"IN", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"IN", 0, 8, 15, 0, 0, 0, 0, L"Independence Day" }, { L"IN", 0, 10, 2, 0, 0, 0, 0, L"Gandhi Jayanti" },
    { L"IN", 0, 12, 25, 0, 0, 0, 0, L"Christmas Day" },
    // United Arab Emirates
    { L"AE", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" }, { L"AE", 0, 12, 1, 0, 0, 0, 0, L"Commemoration Day" },
    { L"AE", 0, 12, 2, 0, 0, 0, 0, L"National Day" }, { L"AE", 0, 12, 3, 0, 0, 0, 0, L"National Day" },
    // Singapore
    { L"SG", 0, 1, 1, 0, 0, 0, 3, L"New Year's Day" }, { L"SG", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"SG", 0, 5, 1, 0, 0, 0, 3, L"Labour Day" }, { L"SG", 0, 8, 9, 0, 0, 0, 3, L"National Day" },
    { L"SG", 0, 12, 25, 0, 0, 0, 3, L"Christmas Day" },
    // China
    { L"CN", 0, 1, 1, 0, 0, 0, 0, L"New Year's Day" },
    { L"CN", 0, 5, 1, 0, 0, 0, 0, L"Labour Day" }, { L"CN", 0, 5, 2, 0, 0, 0, 0, L"Labour Day holiday" },
    { L"CN", 0, 5, 3, 0, 0, 0, 0, L"Labour Day holiday" },
    { L"CN", 0, 10, 1, 0, 0, 0, 0, L"National Day" }, { L"CN", 0, 10, 2, 0, 0, 0, 0, L"National Day holiday" },
    { L"CN", 0, 10, 3, 0, 0, 0, 0, L"National Day holiday" }, { L"CN", 0, 10, 4, 0, 0, 0, 0, L"National Day holiday" },
    { L"CN", 0, 10, 5, 0, 0, 0, 0, L"National Day holiday" }, { L"CN", 0, 10, 6, 0, 0, 0, 0, L"National Day holiday" },
    { L"CN", 0, 10, 7, 0, 0, 0, 0, L"National Day holiday" },
    // Japan (equinoxes are computed separately)
    { L"JP", 0, 1, 1, 0, 0, 0, 3, L"New Year's Day" }, { L"JP", 0, 1, 2, 0, 0, 0, 0, L"New Year holiday" },
    { L"JP", 0, 1, 3, 0, 0, 0, 0, L"New Year holiday" }, { L"JP", 1, 1, 0, 1, 2, 0, 0, L"Coming of Age Day" },
    { L"JP", 0, 2, 11, 0, 0, 0, 3, L"National Foundation Day" }, { L"JP", 0, 2, 23, 0, 0, 0, 3, L"Emperor's Birthday" },
    { L"JP", 0, 4, 29, 0, 0, 0, 3, L"Shōwa Day" }, { L"JP", 0, 5, 3, 0, 0, 0, 0, L"Constitution Day" },
    { L"JP", 0, 5, 4, 0, 0, 0, 0, L"Greenery Day" }, { L"JP", 0, 5, 5, 0, 0, 0, 3, L"Children's Day" },
    { L"JP", 1, 7, 0, 1, 3, 0, 0, L"Marine Day" }, { L"JP", 0, 8, 11, 0, 0, 0, 3, L"Mountain Day" },
    { L"JP", 1, 9, 0, 1, 3, 0, 0, L"Respect for the Aged Day" }, { L"JP", 1, 10, 0, 1, 2, 0, 0, L"Sports Day" },
    { L"JP", 0, 11, 3, 0, 0, 0, 3, L"Culture Day" }, { L"JP", 0, 11, 23, 0, 0, 0, 3, L"Labour Thanksgiving Day" },
    // Australia (national + most states)
    { L"AU", 0, 1, 1, 0, 0, 0, 2, L"New Year's Day" }, { L"AU", 0, 1, 26, 0, 0, 0, 2, L"Australia Day" },
    { L"AU", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" }, { L"AU", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" },
    { L"AU", 0, 4, 25, 0, 0, 0, 0, L"Anzac Day" }, { L"AU", 1, 6, 0, 1, 2, 0, 0, L"King's Birthday" },
    { L"AU", 0, 12, 25, 0, 0, 0, 2, L"Christmas Day" }, { L"AU", 0, 12, 26, 0, 0, 0, 2, L"Boxing Day" },
    // New Zealand
    { L"NZ", 0, 1, 1, 0, 0, 0, 2, L"New Year's Day" }, { L"NZ", 0, 1, 2, 0, 0, 0, 2, L"Day after New Year's" },
    { L"NZ", 0, 2, 6, 0, 0, 0, 2, L"Waitangi Day" }, { L"NZ", 2, 0, 0, 0, 0, -2, 0, L"Good Friday" },
    { L"NZ", 2, 0, 0, 0, 0, 1, 0, L"Easter Monday" }, { L"NZ", 0, 4, 25, 0, 0, 0, 2, L"Anzac Day" },
    { L"NZ", 1, 6, 0, 1, 1, 0, 0, L"King's Birthday" }, { L"NZ", 1, 10, 0, 1, 4, 0, 0, L"Labour Day" },
    { L"NZ", 0, 12, 25, 0, 0, 0, 2, L"Christmas Day" }, { L"NZ", 0, 12, 26, 0, 0, 0, 2, L"Boxing Day" },
};

// Dates that don't follow a simple rule (lunar calendars). Some Islamic dates may shift by a day.
struct HolDate { const wchar_t* cc; int y, m, d, days; const wchar_t* name; };
static const HolDate kHolDates[] = {
    // Chinese New Year
    { L"CN", 2026, 2, 16, 8, L"Spring Festival" }, { L"CN", 2027, 2, 5, 8, L"Spring Festival" }, { L"CN", 2028, 1, 25, 8, L"Spring Festival" },
    { L"SG", 2026, 2, 17, 2, L"Chinese New Year" }, { L"SG", 2027, 2, 6, 2, L"Chinese New Year" }, { L"SG", 2028, 1, 26, 2, L"Chinese New Year" },
    // Qingming, Dragon Boat, Mid-Autumn (China)
    { L"CN", 2026, 4, 5, 1, L"Qingming Festival" }, { L"CN", 2027, 4, 5, 1, L"Qingming Festival" }, { L"CN", 2028, 4, 4, 1, L"Qingming Festival" },
    { L"CN", 2026, 6, 19, 1, L"Dragon Boat Festival" }, { L"CN", 2027, 6, 9, 1, L"Dragon Boat Festival" }, { L"CN", 2028, 5, 28, 1, L"Dragon Boat Festival" },
    { L"CN", 2026, 9, 25, 1, L"Mid-Autumn Festival" }, { L"CN", 2027, 9, 15, 1, L"Mid-Autumn Festival" }, { L"CN", 2028, 10, 3, 1, L"Mid-Autumn Festival" },
    // Holi, Diwali (India); Deepavali (Singapore)
    { L"IN", 2026, 3, 4, 1, L"Holi" }, { L"IN", 2027, 3, 22, 1, L"Holi" }, { L"IN", 2028, 3, 11, 1, L"Holi" },
    { L"IN", 2026, 11, 8, 1, L"Diwali" }, { L"IN", 2027, 10, 29, 1, L"Diwali" }, { L"IN", 2028, 10, 17, 1, L"Diwali" },
    { L"SG", 2026, 11, 8, 1, L"Deepavali" }, { L"SG", 2027, 10, 29, 1, L"Deepavali" }, { L"SG", 2028, 10, 17, 1, L"Deepavali" },
    // Eid al-Fitr
    { L"IN", 2026, 3, 21, 1, L"Eid al-Fitr" }, { L"IN", 2027, 3, 10, 1, L"Eid al-Fitr" }, { L"IN", 2028, 2, 27, 1, L"Eid al-Fitr" },
    { L"AE", 2026, 3, 19, 4, L"Eid al-Fitr" }, { L"AE", 2027, 3, 9, 4, L"Eid al-Fitr" }, { L"AE", 2028, 2, 26, 4, L"Eid al-Fitr" },
    { L"SG", 2026, 3, 21, 1, L"Hari Raya Puasa" }, { L"SG", 2027, 3, 10, 1, L"Hari Raya Puasa" }, { L"SG", 2028, 2, 27, 1, L"Hari Raya Puasa" },
    // Eid al-Adha (+ Arafat Day in the UAE)
    { L"AE", 2026, 5, 26, 4, L"Arafat Day / Eid al-Adha" }, { L"AE", 2027, 5, 15, 4, L"Arafat Day / Eid al-Adha" }, { L"AE", 2028, 5, 4, 4, L"Arafat Day / Eid al-Adha" },
    { L"SG", 2026, 5, 27, 1, L"Hari Raya Haji" }, { L"SG", 2027, 5, 17, 1, L"Hari Raya Haji" }, { L"SG", 2028, 5, 5, 1, L"Hari Raya Haji" },
    // Islamic New Year, Prophet's Birthday (UAE)
    { L"AE", 2026, 6, 16, 1, L"Islamic New Year" }, { L"AE", 2027, 6, 6, 1, L"Islamic New Year" }, { L"AE", 2028, 5, 25, 1, L"Islamic New Year" },
    { L"AE", 2026, 8, 25, 1, L"Prophet's Birthday" }, { L"AE", 2027, 8, 14, 1, L"Prophet's Birthday" }, { L"AE", 2028, 8, 3, 1, L"Prophet's Birthday" },
    // Vesak (Singapore)
    { L"SG", 2026, 5, 31, 1, L"Vesak Day" }, { L"SG", 2027, 5, 20, 1, L"Vesak Day" }, { L"SG", 2028, 5, 9, 1, L"Vesak Day" },
};

static long long DaysOf(int y, int m, int d) {   // days since 1601-01-01
    SYSTEMTIME st{}; st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d; FILETIME f;
    if (!SystemTimeToFileTime(&st, &f)) return 0;
    return (long long)(FtU(f) / (TPM * 1440));
}
static SYSTEMTIME DateOf(long long days) {
    FILETIME f = UFt((ULONGLONG)days * TPM * 1440); SYSTEMTIME st; FileTimeToSystemTime(&f, &st); return st;
}
static int WeekdayOf(long long days) { return (int)(((days + 1) % 7 + 7) % 7); }   // 1601-01-01 was a Monday; 0 = Sunday
static long long EasterDays(int y) {
    int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451, month = (h + l - 7 * m + 114) / 31, day = ((h + l - 7 * m + 114) % 31) + 1;
    return DaysOf(y, month, day);
}
static long long NthWeekday(int y, int m, int wd, int n) {
    if (n > 0) { long long first = DaysOf(y, m, 1); int off = (wd - WeekdayOf(first) + 7) % 7; return first + off + 7 * (n - 1); }
    long long last = DaysOf(m == 12 ? y + 1 : y, m == 12 ? 1 : m + 1, 1) - 1; int off = (WeekdayOf(last) - wd + 7) % 7; return last - off;
}

struct HolDay { long long day; std::wstring name; };
static std::map<std::wstring, std::vector<HolDay>> g_holCache;
static void HolidaysForYear(const std::wstring& cc, int y, std::vector<HolDay>& out) {
    std::vector<HolDay> list;
    for (auto& r : kHolRules) {
        if (_wcsicmp(r.cc, cc.c_str()) != 0) continue;
        long long d = 0;
        if (r.kind == 0) d = DaysOf(y, r.m, r.d);
        else if (r.kind == 1) d = NthWeekday(y, r.m, r.wd, r.n);
        else if (r.kind == 2) d = EasterDays(y) + r.easter;
        else if (r.kind == 3) { long long t = DaysOf(y, r.m, r.d); d = t - ((WeekdayOf(t) - r.wd + 7) % 7); }
        int wd = WeekdayOf(d);
        if (r.obs == 1) { if (wd == 6) d -= 1; else if (wd == 0) d += 1; }
        else if (r.obs == 2) { if (wd == 6) d += 2; else if (wd == 0) d += 1; }
        else if (r.obs == 3) { if (wd == 0) d += 1; }
        // UK-style Christmas/Boxing pairs: don't let both land on the same substitute day
        for (auto& e : list) if (e.day == d && r.obs == 2) d += 1;
        list.push_back({ d, r.name });
    }
    for (auto& h : kHolDates)
        if (_wcsicmp(h.cc, cc.c_str()) == 0 && h.y == y)
            for (int k = 0; k < h.days; k++) list.push_back({ DaysOf(h.y, h.m, h.d) + k, h.name });
    // Japanese equinoxes (valid 1980-2099)
    if (_wcsicmp(cc.c_str(), L"JP") == 0) {
        int v = (int)floor(20.8431 + 0.242194 * (y - 1980) - floor((y - 1980) / 4.0));
        int a = (int)floor(23.2488 + 0.242194 * (y - 1980) - floor((y - 1980) / 4.0));
        list.push_back({ DaysOf(y, 3, v), L"Vernal Equinox Day" });
        list.push_back({ DaysOf(y, 9, a), L"Autumnal Equinox Day" });
    }
    // user additions from WinClock.ini [Holidays]:  CC=YYYY-MM-DD Name   (CC may be * for every country)
    if (!g_ini.empty()) {
        std::vector<wchar_t> buf(32768);
        DWORD n = GetPrivateProfileSectionW(L"Holidays", buf.data(), (DWORD)buf.size(), g_ini.c_str());
        for (const wchar_t* p = buf.data(); p < buf.data() + n && *p; p += wcslen(p) + 1) {
            std::wstring line = p; size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring key = line.substr(0, eq), val = line.substr(eq + 1);
            if (key != L"*" && _wcsicmp(key.c_str(), cc.c_str()) != 0) continue;
            int yy, mm, dd; if (swscanf(val.c_str(), L"%d-%d-%d", &yy, &mm, &dd) != 3 || yy != y) continue;
            size_t sp = val.find(L' ');
            list.push_back({ DaysOf(yy, mm, dd), sp == std::wstring::npos ? L"Holiday" : val.substr(sp + 1) });
        }
    }
    out.swap(list);
}
static const HolDay* HolidayOn(const std::wstring& cc, long long day) {
    if (cc.empty()) return nullptr;
    int y = DateOf(day).wYear;
    for (int yy = y; yy <= y + 1; yy++) {   // observed dates can spill into January
        std::wstring key = cc + L":" + std::to_wstring(yy);
        auto it = g_holCache.find(key);
        if (it == g_holCache.end()) { std::vector<HolDay> v; HolidaysForYear(cc, yy, v); it = g_holCache.emplace(key, std::move(v)).first; }
        for (auto& h : it->second) if (h.day == day) return &h;
    }
    return nullptr;
}
static void ClearHolidayCache() { g_holCache.clear(); }

// a sensible default country for a Windows time zone
static std::wstring CountryForZone(const std::wstring& tz) {
    static const std::pair<const wchar_t*, const wchar_t*> m[] = {
        { L"Eastern Standard Time", L"US" }, { L"Central Standard Time", L"US" }, { L"Mountain Standard Time", L"US" },
        { L"US Mountain Standard Time", L"US" }, { L"Pacific Standard Time", L"US" }, { L"Alaskan Standard Time", L"US" },
        { L"Hawaiian Standard Time", L"US" }, { L"Newfoundland Standard Time", L"CA" }, { L"Atlantic Standard Time", L"CA" },
        { L"Canada Central Standard Time", L"CA" }, { L"Central Standard Time (Mexico)", L"MX" }, { L"Pacific Standard Time (Mexico)", L"MX" },
        { L"Mountain Standard Time (Mexico)", L"MX" }, { L"Eastern Standard Time (Mexico)", L"MX" }, { L"E. South America Standard Time", L"BR" },
        { L"GMT Standard Time", L"GB" }, { L"W. Europe Standard Time", L"DE" }, { L"Romance Standard Time", L"FR" },
        { L"India Standard Time", L"IN" }, { L"Arabian Standard Time", L"AE" }, { L"Singapore Standard Time", L"SG" },
        { L"China Standard Time", L"CN" }, { L"Tokyo Standard Time", L"JP" }, { L"AUS Eastern Standard Time", L"AU" },
        { L"E. Australia Standard Time", L"AU" }, { L"Cen. Australia Standard Time", L"AU" }, { L"W. Australia Standard Time", L"AU" },
        { L"AUS Central Standard Time", L"AU" }, { L"Tasmania Standard Time", L"AU" }, { L"New Zealand Standard Time", L"NZ" },
    };
    for (auto& p : m) if (_wcsicmp(p.first, tz.c_str()) == 0) return p.second;
    return L"";
}
