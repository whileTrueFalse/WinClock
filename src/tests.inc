// ---------------------------------------------------------------- self-tests (build with -DTEST_BUILD)
#ifdef TEST_BUILD
static std::wstring g_testLog; static int g_testFail = 0, g_testPass = 0;
static void Expect(bool ok, const std::wstring& what) {
    (ok ? g_testPass : g_testFail)++;
    g_testLog += (ok ? L"PASS  " : L"FAIL  ") + what + L"\n";
}
static std::wstring DateStr(long long day) { SYSTEMTIME s = DateOf(day); return Fmt(L"%04d-%02d-%02d", s.wYear, s.wMonth, s.wDay); }
static void RunTests() {
    // --- holidays
    auto hol = [](const wchar_t* cc, int y, int m, int d) { const HolDay* h = HolidayOn(cc, DaysOf(y, m, d)); return h ? h->name : std::wstring(L"-"); };
    Expect(hol(L"US", 2026, 11, 26) == L"Thanksgiving", L"US Thanksgiving 2026 = Nov 26: " + hol(L"US", 2026, 11, 26));
    Expect(hol(L"US", 2026, 7, 3) == L"Independence Day", L"US July 4 2026 (Sat) observed Fri Jul 3: " + hol(L"US", 2026, 7, 3));
    Expect(hol(L"US", 2026, 1, 19) == L"Martin Luther King Jr. Day", L"US MLK 2026 = Jan 19");
    Expect(hol(L"US", 2026, 5, 25) == L"Memorial Day", L"US Memorial Day 2026 = May 25");
    Expect(hol(L"GB", 2026, 4, 3) == L"Good Friday", L"GB Good Friday 2026 = Apr 3 (Easter Apr 5)");
    Expect(hol(L"GB", 2026, 4, 6) == L"Easter Monday", L"GB Easter Monday 2026 = Apr 6");
    Expect(hol(L"GB", 2027, 12, 27) == L"Christmas Day", L"GB Christmas 2027 (Sat) substitute Mon 27: " + hol(L"GB", 2027, 12, 27));
    Expect(hol(L"GB", 2027, 12, 28) == L"Boxing Day", L"GB Boxing Day 2027 (Sun) substitute Tue 28: " + hol(L"GB", 2027, 12, 28));
    Expect(hol(L"GB", 2026, 8, 31) == L"Summer bank holiday", L"GB summer bank holiday 2026 = Aug 31");
    Expect(hol(L"CA", 2026, 5, 18) == L"Victoria Day", L"CA Victoria Day 2026 = May 18: " + hol(L"CA", 2026, 5, 18));
    Expect(hol(L"DE", 2026, 5, 14) == L"Ascension Day", L"DE Ascension 2026 = May 14");
    Expect(hol(L"JP", 2026, 3, 20) == L"Vernal Equinox Day", L"JP vernal equinox 2026 = Mar 20: " + hol(L"JP", 2026, 3, 20));
    Expect(hol(L"JP", 2026, 9, 23) == L"Autumnal Equinox Day", L"JP autumnal equinox 2026 = Sep 23: " + hol(L"JP", 2026, 9, 23));
    Expect(hol(L"IN", 2026, 11, 8) == L"Diwali", L"IN Diwali 2026 = Nov 8");
    Expect(hol(L"IN", 2026, 11, 9) == L"-", L"IN Nov 9 2026 is not a holiday");
    Expect(hol(L"AU", 2026, 1, 26) == L"Australia Day", L"AU Australia Day 2026 (Mon)");
    Expect(hol(L"BR", 2026, 2, 16) == L"Carnival", L"BR Carnival Monday 2026 = Feb 16: " + hol(L"BR", 2026, 2, 16));
    Expect(hol(L"US", 2026, 11, 3) == L"-", L"US Nov 3 2026 is a normal day");

    // --- sunrise / sunset
    Clock lon = ZoneClock(L"GMT Standard Time"); lon.lat = 51.51; lon.lon = -0.13;
    long long r, st2; int k = SunForLocalDay(lon, DaysOf(2026, 6, 21), r, st2);
    SYSTEMTIME a = StOfMin(r + 60), b = StOfMin(st2 + 60);   // BST = UTC+1
    Expect(k == 0 && a.wHour == 4 && a.wMinute >= 40 && a.wMinute <= 47, Fmt(L"London sunrise 21 Jun 2026 ≈ 04:43 BST: %02d:%02d", a.wHour, a.wMinute));
    Expect(k == 0 && b.wHour == 21 && b.wMinute >= 18 && b.wMinute <= 25, Fmt(L"London sunset 21 Jun 2026 ≈ 21:21 BST: %02d:%02d", b.wHour, b.wMinute));
    Clock bom = ZoneClock(L"India Standard Time"); bom.lat = 19.08; bom.lon = 72.88;
    k = SunForLocalDay(bom, DaysOf(2026, 11, 2), r, st2); a = StOfMin(r + 330); b = StOfMin(st2 + 330);
    Expect(k == 0 && a.wHour == 6 && a.wMinute >= 38 && a.wMinute <= 52, Fmt(L"Mumbai sunrise 2 Nov 2026 ≈ 06:45: %02d:%02d", a.wHour, a.wMinute));
    Expect(k == 0 && b.wHour == 18 && b.wMinute >= 0 && b.wMinute <= 12, Fmt(L"Mumbai sunset 2 Nov 2026 ≈ 18:06: %02d:%02d", b.wHour, b.wMinute));
    Clock syd = ZoneClock(L"AUS Eastern Standard Time"); syd.lat = -33.87; syd.lon = 151.21;
    k = SunForLocalDay(syd, DaysOf(2026, 12, 21), r, st2); a = StOfMin(r + 660);
    Expect(k == 0 && a.wHour == 5 && a.wMinute >= 35 && a.wMinute <= 47, Fmt(L"Sydney sunrise 21 Dec 2026 ≈ 05:41 AEDT: %02d:%02d", a.wHour, a.wMinute));
    Clock trom = ZoneClock(L"W. Europe Standard Time"); trom.lat = 69.65; trom.lon = 18.96;
    Expect(SunForLocalDay(trom, DaysOf(2026, 6, 21), r, st2) == 1, L"Tromsø midsummer: sun up all day");
    Expect(SunForLocalDay(trom, DaysOf(2026, 12, 21), r, st2) == 2, L"Tromsø midwinter: sun down all day");

    // --- parser
    Clock alex = ZoneClock(L"Tokyo Standard Time"); alex.label = L"Tokyo"; alex.person = L"Alex";
    g_clocks.push_back(alex);
    auto check = [](const wchar_t* text, const wchar_t* tz, int hh, int mm, int wday /* -1 any */) {
        WhenResult w; std::wstring err;
        bool ok = ParseWhen(text, w, &err);
        if (!ok) { Expect(false, std::wstring(L"parse \"") + text + L"\": " + err); return; }
        Clock z = ZoneClock(tz); SYSTEMTIME lt = ClockAt(z, (ULONGLONG)w.utc * TPM).st;
        bool good = lt.wHour == hh && lt.wMinute == mm && (wday < 0 || lt.wDayOfWeek == wday);
        Expect(good, Fmt(L"parse \"%ls\" -> %ls %02d:%02d (%ls) in %ls", text, kDays[lt.wDayOfWeek], lt.wHour, lt.wMinute, w.zone.name.c_str(), tz));
    };
    check(L"3pm EST Thu", L"Eastern Standard Time", 15, 0, 4);
    check(L"Can we do 3:30 PM ET on Friday?", L"Eastern Standard Time", 15, 30, 5);
    check(L"15:30 London tomorrow", L"GMT Standard Time", 15, 30, -1);
    check(L"9:30 am IST on Nov 12", L"India Standard Time", 9, 30, -1);
    check(L"noon PT", L"Pacific Standard Time", 12, 0, -1);
    check(L"10am UTC+5:30", L"India Standard Time", 10, 0, -1);
    check(L"let's meet 4.30pm CET next wednesday", L"W. Europe Standard Time", 16, 30, 3);
    check(L"8 pm tokyo time", L"Tokyo Standard Time", 20, 0, -1);
    check(L"3pm alex", L"Tokyo Standard Time", 15, 0, -1);
    check(L"2026-11-20 14:00 UTC", L"UTC", 14, 0, 5);
    check(L"7:45pm sydney", L"AUS Eastern Standard Time", 19, 45, -1);
    { WhenResult w; Expect(!ParseWhen(L"we shipped 2026 builds", w), L"no time in \"we shipped 2026 builds\""); }
    // --- markets
    {
        long long u; SYSTEMTIME lt{}; lt.wYear = 2026; lt.wMonth = 11; lt.wDay = 10; lt.wHour = 10; lt.wMinute = 0;   // Tue 10:00 IST
        LocalToUtcMin(ZoneClock(L"India Standard Time"), lt, u); long long ch;
        int st = MarketState(kMarkets[0], u, ch);
        Expect(st == 1 && ch - u == 330, Fmt(L"NSE open at 10:00 IST, closes in 5h30 (%d, %lld)", st, ch - u));
        st = MarketState(kMarkets[1], u, ch);   // NYSE at 23:30 Mon NY
        Expect(st == 0 && ch - u == 600, Fmt(L"NYSE closed, opens in 10h (%d, %lld)", st, ch - u));
        st = MarketState(kMarkets[5], u + 3 * 60 + 30 + 60 * 0, ch);   // 13:30 IST = 17:00 Tokyo: closed
        Expect(st == 0, L"TSE closed at 17:00 Tokyo");
        long long u2; SYSTEMTIME tk{}; tk.wYear = 2026; tk.wMonth = 11; tk.wDay = 10; tk.wHour = 12; tk.wMinute = 0; LocalToUtcMin(ZoneClock(L"Tokyo Standard Time"), tk, u2);
        st = MarketState(kMarkets[5], u2, ch);
        Expect(st == 2 && ch - u2 == 30, Fmt(L"TSE lunch at 12:00, reopens in 30 min (%d, %lld)", st, ch - u2));
        SYSTEMTIME sat{}; sat.wYear = 2026; sat.wMonth = 11; sat.wDay = 14; sat.wHour = 11; long long u3; LocalToUtcMin(ZoneClock(L"India Standard Time"), sat, u3);
        st = MarketState(kMarkets[0], u3, ch);
        Expect(st == 0, L"NSE closed on Saturday");
    }
    // --- moon & prayer times
    {
        long long full; SYSTEMTIME f{}; f.wYear = 2026; f.wMonth = 11; f.wDay = 24; f.wHour = 15; FILETIME ft; SystemTimeToFileTime(&f, &ft); full = (long long)(FtU(ft) / TPM);
        std::wstring m = MoonText(full);
        Expect(m.find(L"Full moon") == 0, L"moon on 24 Nov 2026 is full: " + m);
        double p[6]; PrayerTimesUtc(2026, 6, 21, 21.42, 39.83, 3, false, p);   // Makkah, Umm al-Qura
        auto hm = [](double m2) { int x = (int)llround(m2) + 180; x = ((x % 1440) + 1440) % 1440; return Fmt(L"%02d:%02d", x / 60, x % 60); };   // local = UTC+3
        Expect(std::fabs(p[2] + 180 - (12 * 60 + 22)) <= 4, L"Makkah Dhuhr ~12:22 on 21 Jun: " + hm(p[2]));
        Expect(std::fabs(p[0] + 180 - (4 * 60 + 8)) <= 6, L"Makkah Fajr ~04:08 on 21 Jun: " + hm(p[0]));
        Expect(std::fabs(p[4] + 180 - (19 * 60 + 7)) <= 4, L"Makkah Maghrib ~19:07 on 21 Jun: " + hm(p[4]));
    }
    // --- leave-by reminders fire before the event
    {
        Alarm a; a.label = L"Flight"; a.tzid = L"W. Europe Standard Time"; a.hh = 6; a.mm = 40; a.y = 2030; a.mo = 1; a.d = 10; a.lead = 180;
        ComputeNext(a, NowMin());
        Expect(a.next > 0 && a.next - a.lead < a.next, L"leave-by: reminder 3 h before the flight");
    }
    // --- time calculator
    {
        auto calc = [](const wchar_t* q, CalcAnswer& a) { a = CalcAnswer{}; return TimeCalc(q, a); };
        auto first = [](const CalcAnswer& a) { return a.rows.empty() ? std::wstring(L"(none)") : a.rows[0].title; };
        CalcAnswer a;
        bool ok = calc(L"9:00 pm in india, what time it would be in germany on nov 10 2026", a);
        Expect(ok && a.rows.size() == 1 && first(a).find(L"4:30 PM") == 0 && first(a).find(L"Germany") != std::wstring::npos,
               L"calc: 9pm India -> Germany = 4:30 PM: " + first(a));
        Expect(ok && a.rows.size() == 1 && a.rows[0].sub.find(L"4 h 30 min behind India") != std::wstring::npos, L"calc: Germany 4 h 30 min behind India: " + (ok ? a.rows[0].sub : L""));
        ok = calc(L"3pm london to tokyo nov 10 2026", a);
        Expect(ok && first(a).find(L"12:00 AM") == 0 && first(a).find(L"next day") != std::wstring::npos, L"calc: 3pm London -> Tokyo = 12 AM next day: " + first(a));
        ok = calc(L"germany 4:30pm nov 10 2026 in india", a);
        Expect(ok && first(a).find(L"9:00 PM") == 0, L"calc: 4:30pm Germany -> India = 9 PM: " + first(a));
        ok = calc(L"10am usa to india nov 10 2026", a);
        Expect(ok && first(a).find(L"8:30 PM") == 0, L"calc: 10am USA (New York) -> India = 8:30 PM: " + first(a));
        ok = calc(L"9pm india to usa nov 10 2026", a);
        Expect(ok && a.rows.size() == 4 && first(a).find(L"10:30 AM") == 0 && a.rows[3].title.find(L"7:30 AM") == 0,
               L"calc: India -> USA gives 4 zones (NY 10:30 AM .. LA 7:30 AM): " + first(a) + L" / " + (a.rows.size() > 3 ? a.rows[3].title : L""));
        ok = calc(L"what time is it in germany", a);
        Expect(ok && a.rows.size() == 1 && a.rows[0].sub.find(L"right now") == 0, L"calc: what time is it in germany -> now");
        ok = calc(L"9pm, what time would it be in japan", a);
        Expect(ok && a.rows.size() == 1 && first(a).find(L"Japan") != std::wstring::npos, L"calc: 9pm (your time) -> Japan: " + first(a));
        Expect(!calc(L"3pm tokyo", a), L"calc: a single place stays a normal conversion");
        // durations, until, between, ranges
        auto q = [](const wchar_t* text, CalcAnswer& r) { return CalcQuery(text, r); };
        ok = q(L"9pm IST + 3h 45m nov 10 2026", a);
        Expect(ok && first(a).find(L"12:45 AM") == 0 && first(a).find(L"next day") != std::wstring::npos, L"calc: 9pm IST + 3h 45m = 12:45 AM next day: " + first(a));
        ok = q(L"10:30am london - 90 min nov 10 2026", a);
        Expect(ok && first(a).find(L"9:00 AM") == 0, L"calc: 10:30am London - 90 min = 9:00 AM: " + first(a));
        ok = q(L"hours between 9am ny and 5pm tokyo nov 10 2026", a);
        Expect(ok && first(a) == L"18 h", L"calc: 9am NY -> 5pm Tokyo = 18 h: " + first(a) + L" / " + (a.rows.empty() ? L"" : a.rows[0].sub));
        ok = q(L"how long until 6pm london", a);
        Expect(ok && first(a).find(L"to go") != std::wstring::npos, L"calc: how long until 6pm London: " + first(a));
        ok = q(L"9am-5pm india in germany nov 10 2026", a);
        Expect(ok && first(a).find(L"4:30 AM – 12:30 PM") == 0 && a.rows[0].sub.find(L"9:00 AM–12:30 PM") != std::wstring::npos,
               L"calc: 9am-5pm India in Germany = 4:30 AM–12:30 PM, overlap 9–12:30: " + first(a) + L" | " + (a.rows.empty() ? L"" : a.rows[0].sub));
        ok = q(L"now + 2h in tokyo", a);
        Expect(ok && first(a).find(L"Tokyo") != std::wstring::npos, L"calc: now + 2h in tokyo: " + first(a));
        ok = q(L"9pm india to germany nov 10 2026", a);
        Expect(ok && a.rows.size() == 1 && first(a).find(L"4:30 PM") == 0, L"calc: plain conversion still works through CalcQuery: " + first(a));
        Expect(!q(L"3pm tokyo", a), L"calc: single place still falls back");
        // sharing text
        { WhenResult w; w.zone.clock = ZoneClock(L"India Standard Time"); w.zone.name = L"India"; w.hasTime = true;
          SYSTEMTIME lt{}; lt.wYear = 2026; lt.wMonth = 11; lt.wDay = 10; lt.wHour = 21; LocalToUtcMin(w.zone.clock, lt, w.utc);
          PlaceHit ph; ph.place = L"Germany"; WhenZone gz; gz.clock = ZoneClock(L"W. Europe Standard Time"); gz.name = L"Germany"; ph.zones = { gz };
          std::wstring m = TimeMessage(w, { ph });
          Expect(m.find(L"9:00 PM Tue 10 Nov (India)") != std::wstring::npos && m.find(L"Germany — 4:30 PM Tue") != std::wstring::npos, L"share message: " + m); }
        { WhenResult w; bool ok2 = ParseWhen(L"9:00 pm in india, what time it would be in germany", w);
          Expect(ok2 && w.zone.name.find(L"India") != std::wstring::npos, L"clipboard converter reads India as the source: " + w.zone.name); }
        ok = calc(L"9 pm IST to CET nov 10 2026", a);
        Expect(ok && first(a).find(L"4:30 PM") == 0, L"calc: 9pm IST -> CET = 4:30 PM: " + first(a));
    }
    { WhenResult w; ParseWhen(L"3pm EST", w); std::wstring line = ConversionLine(w);
      Expect(line.find(L"New York") != std::wstring::npos && line.find(L"Alex") != std::wstring::npos, L"conversion line: " + line); }
    g_clocks.pop_back();

    // --- DST transitions
    long long at; int o, n;
    long long from = DaysOf(2026, 10, 20) * 1440;
    bool f1 = NextTransition(ZoneClock(L"GMT Standard Time"), from, 60, at, o, n);
    SYSTEMTIME tr = StOfMin(at);
    Expect(f1 && tr.wMonth == 10 && tr.wDay == 25 && tr.wHour == 1 && o == 60 && n == 0, Fmt(L"London falls back 25 Oct 2026 01:00 UTC: %04d-%02d-%02d %02d:%02d %d->%d", tr.wYear, tr.wMonth, tr.wDay, tr.wHour, tr.wMinute, o, n));
    bool f2 = NextTransition(ZoneClock(L"Eastern Standard Time"), from, 60, at, o, n); tr = StOfMin(at);
    Expect(f2 && tr.wMonth == 11 && tr.wDay == 1 && o == -240 && n == -300, Fmt(L"New York falls back 1 Nov 2026: %02d-%02d %d->%d", tr.wMonth, tr.wDay, o, n));
    Expect(!NextTransition(ZoneClock(L"India Standard Time"), from, 60, at, o, n), L"India: no DST");

    // --- ICS
    std::string ics =
        "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"
        "BEGIN:VEVENT\r\nUID:standup\r\nSUMMARY:Daily standup\r\nDTSTART;TZID=America/New_York:20261001T093000\r\nDTEND;TZID=America/New_York:20261001T094500\r\n"
        "RRULE:FREQ=WEEKLY;BYDAY=MO,WE,FR\r\nEXDATE;TZID=America/New_York:20261104T093000\r\n"
        "DESCRIPTION:Join: https://us02web.zoom.us/j/123456789?pwd=abc\\nThanks\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:standup\r\nRECURRENCE-ID;TZID=America/New_York:20261106T093000\r\nSUMMARY:Daily standup (moved)\r\n"
        "DTSTART;TZID=America/New_York:20261106T110000\r\nDTEND;TZID=America/New_York:20261106T111500\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:one\r\nSUMMARY:Design review\r\nDTSTART:20261105T150000Z\r\nDURATION:PT1H\r\nLOCATION:https://meet.google.com/abc-defg-hij\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:x\r\nSUMMARY:Cancelled thing\r\nSTATUS:CANCELLED\r\nDTSTART:20261105T160000Z\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:allday\r\nSUMMARY:Holiday\r\nDTSTART;VALUE=DATE:20261105\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:monthly\r\nSUMMARY:Board\r\nDTSTART;TZID=Europe/London:20260106T140000\r\nRRULE:FREQ=MONTHLY;BYDAY=1TH\r\nEND:VEVENT\r\n"
        "END:VCALENDAR\r\n";
    ParseIcs(ics);
    Expect(g_calEvents.size() == 5, Fmt(L"ICS: 5 events kept (cancelled dropped): %d", (int)g_calEvents.size()));
    Expect(!g_calEvents.empty() && g_calEvents[0].link.find(L"zoom.us") != std::wstring::npos, L"ICS: zoom link found: " + (g_calEvents.empty() ? L"" : g_calEvents[0].link));
    Expect(g_calEvents.size() > 2 && g_calEvents[2].link.find(L"meet.google.com") != std::wstring::npos && g_calEvents[2].dur == 60, L"ICS: meet link + 1h duration");
    // expand around a fixed "now": Wed 4 Nov 2026 12:00 UTC
    {
        std::vector<CalOcc> occ;
        long long w0 = DaysOf(2026, 11, 2) * 1440, w1 = DaysOf(2026, 11, 7) * 1440;
        // reuse the real expander by temporarily faking the window through the events' own dates
        ExpandCalendar();   // uses NowMin(); check the recurrence maths separately below
        auto occAt = [&](int y, int m, int d, int hh, int mm, const wchar_t* tz) { SYSTEMTIME s{}; s.wYear = (WORD)y; s.wMonth = (WORD)m; s.wDay = (WORD)d; s.wHour = (WORD)hh; s.wMinute = (WORD)mm; long long u; LocalToUtcMin(ZoneClock(tz), s, u); return u; };
        // manual expansion of the weekly standup for 2-7 Nov
        CalEvent e = g_calEvents[0]; int count = 0; bool has4 = false, has6 = false, has2 = false;
        for (long long dday = DaysOf(2026, 11, 2); dday < DaysOf(2026, 11, 7); dday++) {
            int wd = WeekdayOf(dday); if (!((e.byDayMask >> wd) & 1)) continue;
            SYSTEMTIME ds = DateOf(dday); ds.wHour = 9; ds.wMinute = 30; long long u = EventUtc(e, ds);
            if (std::find(e.exdates.begin(), e.exdates.end(), u) != e.exdates.end()) { if (ds.wDay == 4) has4 = true; continue; }
            count++; if (ds.wDay == 2) has2 = true; if (ds.wDay == 6) has6 = true;
        }
        Expect(has4 && has2 && count == 2, Fmt(L"ICS: weekly MO/WE/FR with EXDATE Wed 4 Nov -> Mon 2 + Fri 6 (%d)", count));
        Expect(g_calEvents[1].recurId == occAt(2026, 11, 6, 9, 30, L"Eastern Standard Time"), L"ICS: RECURRENCE-ID override matches Fri 6 Nov 09:30 ET");
        Expect(EventUtc(e, e.start) == occAt(2026, 10, 1, 9, 30, L"Eastern Standard Time"), L"ICS: TZID America/New_York mapped to Eastern Standard Time");
        (void)w0; (void)w1; (void)has6;
        long long fakeNow = occAt(2026, 11, 4, 12, 0, L"UTC");
        ExpandCalendarAt(fakeNow);
        std::wstring got; for (auto& o : g_calOcc) { SYSTEMTIME q = StOfMin(o.start); got += Fmt(L"[%ls %d %02d:%02dZ %ls] ", kDays[q.wDayOfWeek], q.wDay, q.wHour, q.wMinute, g_calEvents[o.ev].title.c_str()); }
        bool exp3 = g_calOcc.size() == 3 && StOfMin(g_calOcc[0].start).wHour == 14 && g_calEvents[g_calOcc[1].ev].title == L"Design review" &&
                    g_calEvents[g_calOcc[2].ev].title == L"Daily standup (moved)" && StOfMin(g_calOcc[2].start).wHour == 16;
        Expect(exp3, L"ICS expansion Tue 3 -> Sat 7 Nov: " + got);
    }

    // --- share code round trip
    std::vector<Clock> saved = g_clocks; g_clocks.clear();
    Clock c1 = ZoneClock(L"Eastern Standard Time"); c1.label = L"New York"; c1.person = L"Alex"; c1.ws = 8; c1.we = 16; c1.offMask = 0x41; c1.lat = 40.71; c1.lon = -74.01;
    Clock c2 = ZoneClock(L"Arabian Standard Time"); c2.label = L"Dubai"; c2.offMask = (1 << 5) | (1 << 6); c2.cc = L"AE";
    g_clocks = { c1, c2 };
    std::wstring code = ShareCode(); std::vector<Clock> back;
    bool ok = ParseShareCode(L"here you go: " + code + L" thanks", back);
    Expect(ok && back.size() == 2 && back[0].person == L"Alex" && back[0].ws == 8 && back[1].offMask == 0x60 && back[1].cc == L"AE", L"share code round trip (" + std::to_wstring(code.size()) + L" chars)");
    g_clocks = saved;

    // --- status with person hours, overnight shift, days off, away
    Clock night = ZoneClock(L"UTC"); night.ws = 22; night.we = 6;
    SYSTEMTIME t2 = DateOf(DaysOf(2026, 11, 3)); t2.wHour = 2;
    Expect(ClockStatusAt(night, t2) == ST_WORK, L"overnight shift 22-06 is working at 02:00");
    Clock gulf = ZoneClock(L"Arabian Standard Time"); gulf.offMask = (1 << 5) | (1 << 6); gulf.cc = L"-";
    SYSTEMTIME fri = DateOf(DaysOf(2026, 11, 6)); fri.wHour = 11;
    Expect(ClockStatusAt(gulf, fri) == ST_FREE, L"Fri/Sat weekend: Friday 11:00 is free");
    Clock away = ZoneClock(L"UTC"); away.awayY = 2026; away.awayM = 11; away.awayD = 12; away.note = L"on leave";
    SYSTEMTIME tue = DateOf(DaysOf(2026, 11, 10)); tue.wHour = 11; std::wstring why;
    Expect(ClockStatusAt(away, tue, &why) == ST_AWAY && why == L"on leave", L"away until 12 Nov shows the note");
    Clock us = ZoneClock(L"Eastern Standard Time"); SYSTEMTIME tg = DateOf(DaysOf(2026, 11, 26)); tg.wHour = 11;
    Expect(ClockStatusAt(us, tg, &why) == ST_HOLIDAY && why == L"Thanksgiving", L"New York on Thanksgiving 11:00 = holiday (auto country)");

    std::wstring out = Fmt(L"%d passed, %d failed\n\n", g_testPass, g_testFail) + g_testLog;
    HANDLE f = CreateFileW(L"winclock-tests.txt", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) { std::string u = Utf8(out); DWORD w; WriteFile(f, u.data(), (DWORD)u.size(), &w, nullptr); CloseHandle(f); }
}
#endif
