// ---------------------------------------------------------------- networking (only when you turn it on)
// Two features touch the network: the update check (on by default; it only asks GitHub for the latest
// version number) and the calendar link (off until you add one).
// Requests run on a short-lived background thread; results come back to the UI thread as WM_APP_NET.

enum { NET_CAL = 1, NET_UPDATE = 2, NET_UPDATE_MANUAL = 3, NET_DOWNLOAD = 4, NET_WEATHER = 5 };
struct NetJob { int kind; std::wstring url; std::string body; DWORD status = 0; };
static volatile LONG g_netBusy[8] = {};

static bool HttpGet(const std::wstring& url, std::string& out, DWORD& status, size_t maxBytes = 4 << 20) {
    URL_COMPONENTS uc{ sizeof(uc) };
    wchar_t host[256] = L"", path[2048] = L"";
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    wchar_t extra[2048] = L""; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
    std::wstring u = url;
    if (u.compare(0, 9, L"webcal://") == 0) u = L"https://" + u.substr(9);
    if (!WinHttpCrackUrl(u.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) return false;   // https only
    HINTERNET s = WinHttpOpen(L"WinClock/" APP_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(L"WinClock/" APP_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;
    WinHttpSetTimeouts(s, 10000, 10000, 15000, 30000);
    bool ok = false;
    if (HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0)) {
        std::wstring full = std::wstring(path) + extra;
        if (HINTERNET r = WinHttpOpenRequest(c, L"GET", full.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
            DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS; WinHttpSetOption(r, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));
            if (WinHttpSendRequest(r, L"Accept: */*\r\n", (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
                DWORD sz = sizeof(status); WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr);
                for (;;) {
                    DWORD avail = 0; if (!WinHttpQueryDataAvailable(r, &avail) || !avail) break;
                    size_t old = out.size(); if (old + avail > maxBytes) break;
                    out.resize(old + avail); DWORD got = 0;
                    if (!WinHttpReadData(r, &out[old], avail, &got)) break;
                    out.resize(old + got);
                }
                ok = status == 200;
            }
            WinHttpCloseHandle(r);
        }
        WinHttpCloseHandle(c);
    }
    WinHttpCloseHandle(s);
    return ok;
}
static DWORD WINAPI NetThread(LPVOID p) {
    NetJob* j = (NetJob*)p;
    if (!HttpGet(j->url, j->body, j->status)) j->body.clear();
    if (!PostMessageW(g_main, WM_APP_NET, (WPARAM)j->kind, (LPARAM)j)) delete j;
    return 0;
}
static void NetStart(int kind, const std::wstring& url) {
#ifdef STORE_BUILD
    if (kind == NET_UPDATE || kind == NET_UPDATE_MANUAL || kind == NET_DOWNLOAD) {   // the Store handles updates
        if (kind == NET_UPDATE_MANUAL) ShellExecuteW(nullptr, L"open", L"ms-windows-store://downloadsandupdates", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
#endif
    if (InterlockedCompareExchange(&g_netBusy[kind], 1, 0) != 0) return;
    NetJob* j = new NetJob{ kind, url };
    HANDLE t = CreateThread(nullptr, 64 * 1024, NetThread, j, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (t) CloseHandle(t); else { delete j; g_netBusy[kind] = 0; }
}

// ---------------------------------------------------------------- calendar (.ics) overlay
struct CalEvent {
    std::wstring uid, title, link, tzid;
    bool allDay = false, utc = false, floating = false;
    SYSTEMTIME start{};           // wall time in tzid (or UTC / floating)
    int dur = 30;                 // minutes
    std::wstring freq; int interval = 1, count = 0; long long until = -1;
    int byDayMask = 0; std::vector<std::pair<int, int>> byDayOrd;   // (ordinal, weekday) for MONTHLY BYDAY=2TU
    std::vector<int> byMonthDay;
    std::vector<long long> exdates;   // UTC minutes
    long long recurId = -1;           // set on an edited single instance of a series
};
struct CalOcc { long long start, end; int ev; };
static std::vector<CalEvent> g_calEvents;
static std::vector<CalOcc> g_calOcc;
static long long g_calFetched = -1, g_calExpanded = -1;
static std::wstring g_calError;
static std::vector<long long> g_calWarned;
static std::wstring g_balloonUrl;
static std::wstring g_updTag;          // newer release found on GitHub, e.g. "v2.3.0"

static std::wstring IanaToWindows(const std::wstring& z) {
    static const std::pair<const wchar_t*, const wchar_t*> m[] = {
        { L"America/New_York", L"Eastern Standard Time" }, { L"America/Detroit", L"Eastern Standard Time" }, { L"America/Toronto", L"Eastern Standard Time" },
        { L"America/Chicago", L"Central Standard Time" }, { L"America/Winnipeg", L"Central Standard Time" }, { L"America/Denver", L"Mountain Standard Time" },
        { L"America/Edmonton", L"Mountain Standard Time" }, { L"America/Phoenix", L"US Mountain Standard Time" }, { L"America/Los_Angeles", L"Pacific Standard Time" },
        { L"America/Vancouver", L"Pacific Standard Time" }, { L"America/Anchorage", L"Alaskan Standard Time" }, { L"Pacific/Honolulu", L"Hawaiian Standard Time" },
        { L"America/Halifax", L"Atlantic Standard Time" }, { L"America/St_Johns", L"Newfoundland Standard Time" }, { L"America/Regina", L"Canada Central Standard Time" },
        { L"America/Mexico_City", L"Central Standard Time (Mexico)" }, { L"America/Bogota", L"SA Pacific Standard Time" }, { L"America/Lima", L"SA Pacific Standard Time" },
        { L"America/Sao_Paulo", L"E. South America Standard Time" }, { L"America/Argentina/Buenos_Aires", L"Argentina Standard Time" }, { L"America/Buenos_Aires", L"Argentina Standard Time" },
        { L"America/Santiago", L"Pacific SA Standard Time" }, { L"America/Caracas", L"Venezuela Standard Time" },
        { L"Europe/London", L"GMT Standard Time" }, { L"Europe/Dublin", L"GMT Standard Time" }, { L"Europe/Lisbon", L"GMT Standard Time" },
        { L"Europe/Berlin", L"W. Europe Standard Time" }, { L"Europe/Amsterdam", L"W. Europe Standard Time" }, { L"Europe/Rome", L"W. Europe Standard Time" },
        { L"Europe/Zurich", L"W. Europe Standard Time" }, { L"Europe/Vienna", L"W. Europe Standard Time" }, { L"Europe/Stockholm", L"W. Europe Standard Time" },
        { L"Europe/Oslo", L"W. Europe Standard Time" }, { L"Europe/Paris", L"Romance Standard Time" }, { L"Europe/Madrid", L"Romance Standard Time" },
        { L"Europe/Brussels", L"Romance Standard Time" }, { L"Europe/Copenhagen", L"Romance Standard Time" }, { L"Europe/Warsaw", L"Central European Standard Time" },
        { L"Europe/Prague", L"Central Europe Standard Time" }, { L"Europe/Budapest", L"Central Europe Standard Time" }, { L"Europe/Athens", L"GTB Standard Time" },
        { L"Europe/Bucharest", L"GTB Standard Time" }, { L"Europe/Helsinki", L"FLE Standard Time" }, { L"Europe/Kiev", L"FLE Standard Time" }, { L"Europe/Kyiv", L"FLE Standard Time" },
        { L"Europe/Istanbul", L"Turkey Standard Time" }, { L"Europe/Moscow", L"Russian Standard Time" }, { L"Asia/Dubai", L"Arabian Standard Time" },
        { L"Asia/Riyadh", L"Arab Standard Time" }, { L"Asia/Qatar", L"Arab Standard Time" }, { L"Asia/Tehran", L"Iran Standard Time" }, { L"Asia/Karachi", L"Pakistan Standard Time" },
        { L"Asia/Kolkata", L"India Standard Time" }, { L"Asia/Calcutta", L"India Standard Time" }, { L"Asia/Kathmandu", L"Nepal Standard Time" },
        { L"Asia/Dhaka", L"Bangladesh Standard Time" }, { L"Asia/Bangkok", L"SE Asia Standard Time" }, { L"Asia/Jakarta", L"SE Asia Standard Time" },
        { L"Asia/Ho_Chi_Minh", L"SE Asia Standard Time" }, { L"Asia/Singapore", L"Singapore Standard Time" }, { L"Asia/Kuala_Lumpur", L"Singapore Standard Time" },
        { L"Asia/Manila", L"Singapore Standard Time" }, { L"Asia/Shanghai", L"China Standard Time" }, { L"Asia/Hong_Kong", L"China Standard Time" },
        { L"Asia/Taipei", L"Taipei Standard Time" }, { L"Asia/Seoul", L"Korea Standard Time" }, { L"Asia/Tokyo", L"Tokyo Standard Time" },
        { L"Asia/Jerusalem", L"Israel Standard Time" }, { L"Africa/Cairo", L"Egypt Standard Time" }, { L"Africa/Johannesburg", L"South Africa Standard Time" },
        { L"Africa/Lagos", L"W. Central Africa Standard Time" }, { L"Africa/Nairobi", L"E. Africa Standard Time" }, { L"Australia/Sydney", L"AUS Eastern Standard Time" },
        { L"Australia/Melbourne", L"AUS Eastern Standard Time" }, { L"Australia/Brisbane", L"E. Australia Standard Time" }, { L"Australia/Adelaide", L"Cen. Australia Standard Time" },
        { L"Australia/Darwin", L"AUS Central Standard Time" }, { L"Australia/Perth", L"W. Australia Standard Time" }, { L"Australia/Hobart", L"Tasmania Standard Time" },
        { L"Pacific/Auckland", L"New Zealand Standard Time" }, { L"UTC", L"UTC" }, { L"Etc/UTC", L"UTC" }, { L"GMT", L"UTC" }, { L"Etc/GMT", L"UTC" },
    };
    std::wstring k = z;
    size_t p = k.find(L"/mozilla.org/"); if (p != std::wstring::npos) k = k.substr(p + 13);
    for (auto& e : m) if (_wcsicmp(e.first, k.c_str()) == 0) return e.second;
    DYNAMIC_TIME_ZONE_INFORMATION d; if (FindZone(k, d)) return k;   // already a Windows name (Outlook)
    return L"";
}
static bool IcsTime(const std::wstring& v, SYSTEMTIME& st, bool& utc, bool& dateOnly) {
    st = SYSTEMTIME{}; utc = false; dateOnly = false;
    int y, mo, d, h = 0, mi = 0, s = 0;
    if (swscanf(v.c_str(), L"%4d%2d%2d", &y, &mo, &d) != 3) return false;
    if (v.size() >= 15 && v[8] == L'T') { swscanf(v.c_str() + 9, L"%2d%2d%2d", &h, &mi, &s); utc = v.size() >= 16 && (v[15] == L'Z' || v[15] == L'z'); }
    else dateOnly = true;
    st.wYear = (WORD)y; st.wMonth = (WORD)mo; st.wDay = (WORD)d; st.wHour = (WORD)h; st.wMinute = (WORD)mi;
    return y > 1900 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31;
}
static std::wstring IcsUnescape(const std::wstring& s) {
    std::wstring o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\\' && i + 1 < s.size()) { wchar_t n = s[++i]; o += (n == L'n' || n == L'N') ? L'\n' : n; }
        else o += s[i];
    }
    return o;
}
static std::wstring FindJoinLink(const std::wstring& text) {
    static const wchar_t* hosts[] = { L"zoom.us/", L"meet.google.com/", L"teams.microsoft.com/", L"teams.live.com/", L"webex.com/", L"whereby.com/", L"chime.aws/", L"gotomeeting.com/" };
    std::wstring best;
    for (size_t p = text.find(L"https://"); p != std::wstring::npos; p = text.find(L"https://", p + 8)) {
        size_t e = p; while (e < text.size() && !iswspace(text[e]) && text[e] != L'"' && text[e] != L'>' && text[e] != L'<' && text[e] != L',' && text[e] != L';') e++;
        std::wstring url = text.substr(p, e - p);
        for (auto h : hosts) if (url.find(h) != std::wstring::npos) return url;
    }
    return best;
}
static long long EventUtc(const CalEvent& e, const SYSTEMTIME& wall) {
    if (e.utc) { SYSTEMTIME st = wall; FILETIME f; SystemTimeToFileTime(&st, &f); return (long long)(FtU(f) / TPM); }
    Clock z = e.tzid.empty() ? g_localClock : ZoneClock(e.tzid);
    long long u = 0; if (!LocalToUtcMin(z, wall, u)) { FILETIME f; SYSTEMTIME st = wall; SystemTimeToFileTime(&st, &f); u = (long long)(FtU(f) / TPM); }
    return u;
}
static void ParseIcs(const std::string& raw) {
    std::wstring text = FromUtf8(raw);
    // unfold continuation lines
    std::vector<std::wstring> lines; std::wstring cur;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == L'\n') {
            if (!cur.empty() && cur.back() == L'\r') cur.pop_back();
            if (!cur.empty() && (cur[0] == L' ' || cur[0] == L'\t') && !lines.empty()) lines.back() += cur.substr(1);
            else lines.push_back(cur);
            cur.clear();
        } else cur += text[i];
    }
    std::vector<CalEvent> evs; CalEvent e; bool in = false, cancelled = false; std::wstring blob;
    for (auto& ln : lines) {
        if (ln == L"BEGIN:VEVENT") { e = CalEvent(); in = true; cancelled = false; blob.clear(); continue; }
        if (ln == L"END:VEVENT") {
            if (in && !cancelled && e.start.wYear) { if (e.link.empty()) e.link = FindJoinLink(blob); evs.push_back(e); }
            in = false; continue;
        }
        if (!in) continue;
        size_t colon = ln.find(L':'); if (colon == std::wstring::npos) continue;
        std::wstring head = ln.substr(0, colon), val = ln.substr(colon + 1);
        std::wstring name = head.substr(0, head.find(L';')), params = head.find(L';') == std::wstring::npos ? L"" : head.substr(head.find(L';'));
        std::wstring tz; { size_t p = params.find(L"TZID="); if (p != std::wstring::npos) { size_t q = params.find(L';', p + 5); tz = params.substr(p + 5, q == std::wstring::npos ? std::wstring::npos : q - p - 5); if (!tz.empty() && tz[0] == L'"') tz = tz.substr(1, tz.size() - 2); } }
        if (name == L"SUMMARY") e.title = IcsUnescape(val);
        else if (name == L"UID") e.uid = val;
        else if (name == L"STATUS" && val == L"CANCELLED") cancelled = true;
        else if (name == L"DTSTART") { bool u, dOnly; IcsTime(val, e.start, u, dOnly); e.utc = u; e.allDay = dOnly; e.tzid = u ? L"" : IanaToWindows(tz); e.floating = !u && tz.empty(); }
        else if (name == L"DTEND") {
            SYSTEMTIME en; bool u, dOnly;
            if (IcsTime(val, en, u, dOnly) && e.start.wYear) {
                CalEvent tmp = e; tmp.utc = u; tmp.tzid = u ? L"" : IanaToWindows(tz);
                long long a = EventUtc(e, e.start), b = EventUtc(tmp, en);
                e.dur = (int)std::clamp(b - a, 0LL, 7 * 1440LL);
            }
        }
        else if (name == L"DURATION") {
            int total = 0, num = 0;
            for (wchar_t ch : val) {
                if (iswdigit(ch)) { num = num * 10 + (ch - L'0'); continue; }
                if (ch == L'W') total += num * 7 * 1440; else if (ch == L'D') total += num * 1440;
                else if (ch == L'H') total += num * 60; else if (ch == L'M') total += num;
                num = 0;
            }
            e.dur = std::clamp(total, 0, 7 * 1440);
        }
        else if (name == L"RRULE") {
            for (auto& part : Split(val, L';')) {
                size_t eq = part.find(L'='); if (eq == std::wstring::npos) continue;
                std::wstring k = part.substr(0, eq), v = part.substr(eq + 1);
                if (k == L"FREQ") e.freq = v;
                else if (k == L"INTERVAL") e.interval = std::max(1, _wtoi(v.c_str()));
                else if (k == L"COUNT") e.count = _wtoi(v.c_str());
                else if (k == L"UNTIL") { SYSTEMTIME st; bool u, dOnly; if (IcsTime(v, st, u, dOnly)) { if (dOnly) { st.wHour = 23; st.wMinute = 59; } CalEvent t2; t2.utc = u; e.until = EventUtc(t2, st); } }
                else if (k == L"BYMONTHDAY") for (auto& x : Split(v, L',')) e.byMonthDay.push_back(_wtoi(x.c_str()));
                else if (k == L"BYDAY") for (auto& x : Split(v, L',')) {
                    if (x.size() < 2) continue;
                    std::wstring wd = x.substr(x.size() - 2); int ord = x.size() > 2 ? _wtoi(x.substr(0, x.size() - 2).c_str()) : 0;
                    static const wchar_t* ds[] = { L"SU", L"MO", L"TU", L"WE", L"TH", L"FR", L"SA" };
                    for (int k2 = 0; k2 < 7; k2++) if (wd == ds[k2]) { if (ord) e.byDayOrd.push_back({ ord, k2 }); else e.byDayMask |= 1 << k2; }
                }
            }
        }
        else if (name == L"EXDATE") for (auto& x : Split(val, L',')) { SYSTEMTIME st; bool u, dOnly; if (IcsTime(x, st, u, dOnly)) { CalEvent t2 = e; t2.utc = u; t2.tzid = u ? L"" : IanaToWindows(tz); e.exdates.push_back(EventUtc(t2, st)); } }
        else if (name == L"RECURRENCE-ID") { SYSTEMTIME st; bool u, dOnly; if (IcsTime(val, st, u, dOnly)) { CalEvent t2; t2.utc = u; t2.tzid = u ? L"" : IanaToWindows(tz); e.recurId = EventUtc(t2, st); } }
        else if (name == L"URL" || name == L"X-GOOGLE-CONFERENCE" || name == L"X-MICROSOFT-SKYPETEAMSMEETINGURL") { std::wstring l = FindJoinLink(IcsUnescape(val)); if (!l.empty()) e.link = l; }
        if (name == L"DESCRIPTION" || name == L"LOCATION" || name == L"URL") blob += IcsUnescape(val) + L"\n";
    }
    g_calEvents.swap(evs);
}
static void ExpandCalendarAt(long long now) {
    long long w0 = now - 1440, w1 = now + 3 * 1440;
    g_calExpanded = now;
    std::vector<CalOcc> occ;
    // edited single instances replace the matching occurrence of their series
    std::map<std::wstring, std::vector<long long>> overridden;
    for (auto& e : g_calEvents) if (e.recurId >= 0) overridden[e.uid].push_back(e.recurId);
    for (size_t i = 0; i < g_calEvents.size(); i++) {
        const CalEvent& e = g_calEvents[i];
        if (e.allDay) continue;
        long long first = EventUtc(e, e.start);
        if (e.freq.empty() || e.recurId >= 0) {
            if (first + e.dur >= w0 && first <= w1) occ.push_back({ first, first + std::max(e.dur, 1), (int)i });
            continue;
        }
        auto& ov = overridden[e.uid];
        long long d0 = DaysOf(e.start.wYear, e.start.wMonth, e.start.wDay);
        long long dEnd = std::min<long long>(d0 + 366 * 30, w1 / 1440 + 2);
        int n = 0, startWd = WeekdayOf(d0);
        long long weekStart0 = d0 - ((startWd + 6) % 7);   // Monday-based weeks
        for (long long d = d0; d <= dEnd; d++) {
            SYSTEMTIME ds = DateOf(d); int wd = WeekdayOf(d); bool match = false;
            if (e.freq == L"DAILY") match = (d - d0) % e.interval == 0 && (!e.byDayMask || ((e.byDayMask >> wd) & 1));
            else if (e.freq == L"WEEKLY") {
                long long wk = (d - ((wd + 6) % 7) - weekStart0) / 7;
                match = wk % e.interval == 0 && ((e.byDayMask ? (e.byDayMask >> wd) & 1 : wd == startWd));
            } else if (e.freq == L"MONTHLY") {
                int months = (ds.wYear - e.start.wYear) * 12 + ds.wMonth - e.start.wMonth;
                if (months % e.interval == 0) {
                    if (!e.byMonthDay.empty()) { for (int md : e.byMonthDay) if (md == ds.wDay) match = true; }
                    else if (!e.byDayOrd.empty()) {
                        for (auto& o : e.byDayOrd) if (o.second == wd) {
                            int ord = (ds.wDay - 1) / 7 + 1;
                            long long nextMonth = DaysOf(ds.wMonth == 12 ? ds.wYear + 1 : ds.wYear, ds.wMonth == 12 ? 1 : ds.wMonth + 1, 1);
                            int fromEnd = (int)((nextMonth - 1 - d) / 7) + 1;
                            if (o.first == ord || o.first == -fromEnd) match = true;
                        }
                    } else match = ds.wDay == e.start.wDay;
                }
            } else if (e.freq == L"YEARLY") match = ds.wMonth == e.start.wMonth && ds.wDay == e.start.wDay && (ds.wYear - e.start.wYear) % e.interval == 0;
            if (!match) continue;
            SYSTEMTIME wall = ds; wall.wHour = e.start.wHour; wall.wMinute = e.start.wMinute;
            long long u = EventUtc(e, wall);
            if (u < first) continue;
            n++;
            if (e.count && n > e.count) break;
            if (e.until >= 0 && u > e.until) break;
            if (u > w1) break;
            if (u + e.dur < w0) continue;
            if (std::find(e.exdates.begin(), e.exdates.end(), u) != e.exdates.end()) continue;
            if (std::find(ov.begin(), ov.end(), u) != ov.end()) continue;
            occ.push_back({ u, u + std::max(e.dur, 1), (int)i });
        }
    }
    std::sort(occ.begin(), occ.end(), [](const CalOcc& a, const CalOcc& b) { return a.start < b.start; });
    g_calOcc.swap(occ);
}
static void ExpandCalendar() { ExpandCalendarAt(NowMin()); }
static const CalOcc* CalCurrentOrNext() {
    if (!S.calOn) return nullptr;
    long long now = NowMin();
    for (auto& o : g_calOcc) if (o.end > now) return &o;
    return nullptr;
}
static std::wstring CalStatusText(long long horizon) {
    const CalOcc* o = CalCurrentOrNext(); if (!o) return L"";
    long long now = NowMin(); const CalEvent& e = g_calEvents[o->ev];
    std::wstring t = e.title.empty() ? L"(no title)" : e.title;
    if (o->start <= now) return L"now: " + t + L" · ends in " + FmtSpan(std::max(1LL, o->end - now));
    if (o->start - now > horizon) return L"";
    return L"next: " + t + L" in " + FmtSpan(std::max(1LL, o->start - now));
}
static void CalTick() {   // every minute
    if (!S.calOn) return;
    long long now = NowMin();
    if (g_calFetched < 0 || now - g_calFetched >= 15) { g_calFetched = now; NetStart(NET_CAL, S.calUrl); }
    if (g_calExpanded < 0 || now - g_calExpanded >= 60) ExpandCalendar();
    for (auto& o : g_calOcc) {
        long long left = o.start - now;
        if (left < 0 || left > S.calWarn || S.calWarn == 0) continue;
        if (std::find(g_calWarned.begin(), g_calWarned.end(), o.start * 31 + o.ev) != g_calWarned.end()) continue;
        g_calWarned.push_back(o.start * 31 + o.ev);
        const CalEvent& e = g_calEvents[o.ev];
        std::wstring body;
        for (size_t i = 0; i < g_clocks.size() && i < 5; i++) body += DisplayName((int)i) + L"  " + FmtHMFull(ClockAt(g_clocks[i], (ULONGLONG)o.start * TPM).st) + L"\n";
        if (!e.link.empty()) body += L"Click to join";
        g_balloonUrl = e.link; g_balloonUpdate = false;
        Balloon((e.title.empty() ? L"Meeting" : e.title) + (left ? Fmt(L" in %lld min", left) : L" starts now"), body);
    }
    if (g_calWarned.size() > 200) g_calWarned.erase(g_calWarned.begin(), g_calWarned.begin() + 100);
}

// ---------------------------------------------------------------- update check (at startup, then daily)
static bool NewerVersion(const std::wstring& tag, const std::wstring& cur) {
    int a[3] = {}, b[3] = {};
    swscanf(tag.c_str() + (tag.size() && (tag[0] == L'v' || tag[0] == L'V')), L"%d.%d.%d", &a[0], &a[1], &a[2]);
    swscanf(cur.c_str(), L"%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
    return false;
}
static std::wstring JsonField(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\""; size_t p = j.find(k); if (p == std::string::npos) return L"";
    p = j.find('"', j.find(':', p) + 1); if (p == std::string::npos) return L"";
    size_t q = j.find('"', p + 1); if (q == std::string::npos) return L"";
    return FromUtf8(j.substr(p + 1, q - p - 1));
}
static const wchar_t* kReleasesApi = L"https://api.github.com/repos/whileTrueFalse/WinClock/releases/latest";
static const wchar_t* kReleasesPage = L"https://github.com/whileTrueFalse/WinClock/releases/latest";
// Called every minute. The first check happens about a minute after WinClock starts (so the network is up
// after sign-in), then once a day while it keeps running.
static void UpdateTick() {
    static long long lastCheck = -1, started = -1;
    long long now = NowMin();
    if (started < 0) started = now;
    if (!S.updates || now == started) return;
    if (lastCheck >= 0 && now - lastCheck < 1440) return;
    lastCheck = now; S.lastUpdDay = (int)(now / 1440); SaveSettings();
    NetStart(NET_UPDATE, kReleasesApi);
}
static bool SafeTag(const std::wstring& t) {
    if (t.empty() || t.size() > 20) return false;
    for (wchar_t c : t) if (!(iswdigit(c) || c == L'.' || c == L'v' || c == L'V')) return false;
    return true;
}
static void OfferUpdate() {
    if (!SafeTag(g_updTag)) return;
    int r = MessageBoxW(nullptr,
        (L"WinClock " + g_updTag + L" is available. You have " APP_VERSION L".\n\n"
         L"Update now? It downloads in a few seconds and WinClock restarts by itself. Your clocks and settings are kept.\n\n"
         L"Yes: update now     No: later     Cancel: see what's new").c_str(),
        L"WinClock update", MB_YESNOCANCEL | MB_ICONINFORMATION | MB_SETFOREGROUND);
    if (r == IDCANCEL) { ShellExecuteW(nullptr, L"open", (L"https://github.com/whileTrueFalse/WinClock/releases/tag/" + g_updTag).c_str(), nullptr, nullptr, SW_SHOWNORMAL); return; }
    if (r != IDYES) return;
    NetStart(NET_DOWNLOAD, L"https://github.com/whileTrueFalse/WinClock/releases/download/" + g_updTag + L"/WinClock.exe");
}
static std::wstring ExePath() { wchar_t p[MAX_PATH]; DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH); return std::wstring(p, n); }
static void UpdateFailed(const std::wstring& why) {
    if (MessageBoxW(nullptr, (L"The update couldn't be installed: " + why + L"\n\nOpen the download page instead?").c_str(),
                    L"WinClock update", MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND) == IDYES)
        ShellExecuteW(nullptr, L"open", kReleasesPage, nullptr, nullptr, SW_SHOWNORMAL);
}
// Swap the running exe for the downloaded one: Windows lets a running exe be renamed, so the old file
// moves aside to WinClock.exe.old, the new one takes its name, and the new copy starts with /updated.
static void InstallUpdate(const std::string& body) {
    if (body.size() < 100 * 1024 || body.compare(0, 2, "MZ") != 0) { UpdateFailed(L"the download looks incomplete."); return; }
    std::wstring exe = ExePath(), fresh = exe + L".new", old = exe + L".old";
    HANDLE f = CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { UpdateFailed(L"WinClock's folder isn't writable."); return; }
    DWORD wrote = 0; bool ok = WriteFile(f, body.data(), (DWORD)body.size(), &wrote, nullptr) && wrote == body.size();
    CloseHandle(f);
    if (!ok) { DeleteFileW(fresh.c_str()); UpdateFailed(L"the disk may be full."); return; }
    DeleteFileW(old.c_str());
    if (!MoveFileExW(exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(fresh.c_str()); UpdateFailed(L"couldn't replace WinClock.exe."); return; }
    if (!MoveFileExW(fresh.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING); DeleteFileW(fresh.c_str());
        UpdateFailed(L"couldn't replace WinClock.exe."); return;
    }
    // Keep "Apps & features" in step when WinClock was installed with the installer.
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinClock", 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS) {
        wchar_t loc[MAX_PATH] = L""; DWORD sz = sizeof(loc);
        std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
        if (RegQueryValueExW(k, L"InstallLocation", nullptr, nullptr, (BYTE*)loc, &sz) == ERROR_SUCCESS && _wcsicmp(loc, dir.c_str()) == 0) {
            std::wstring v = g_updTag.substr(g_updTag[0] == L'v' || g_updTag[0] == L'V');
            RegSetValueExW(k, L"DisplayVersion", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(k);
    }
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\" /updated";
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxW(nullptr, L"The update is installed. Please start WinClock again.", L"WinClock update", MB_OK | MB_ICONINFORMATION);
    } else { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    PostMessageW(g_main, WM_CLOSE, 0, 0);   // the new copy waits for us to exit, then takes over
}
// After an update: remove the old exe (it may still be closing for a moment).
static void CleanupOldExe() {
    std::wstring old = ExePath() + L".old";
    for (int i = 0; i < 25 && GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES; i++) { if (DeleteFileW(old.c_str())) break; Sleep(200); }
    DeleteFileW((ExePath() + L".new").c_str());
}
static void ParseWeather(const std::string& body);   // panels.inc
static void HandleNet(NetJob* j) {
    g_netBusy[j->kind] = 0;
    if (j->kind == NET_WEATHER) { if (!j->body.empty()) ParseWeather(j->body); delete j; return; }
    if (j->kind == NET_CAL) {
        if (j->body.empty()) g_calError = j->status ? Fmt(L"calendar link returned HTTP %lu", j->status) : L"couldn't reach the calendar link";
        else if (j->body.find("BEGIN:VCALENDAR") == std::string::npos) g_calError = L"that link didn't return a calendar (.ics)";
        else { g_calError.clear(); ParseIcs(j->body); ExpandCalendar(); }
    } else if (j->kind == NET_DOWNLOAD) {
        if (j->body.empty()) UpdateFailed(j->status ? Fmt(L"GitHub returned HTTP %lu.", j->status) : L"couldn't reach GitHub.");
        else InstallUpdate(j->body);
    } else {
        std::wstring tag = JsonField(j->body, "tag_name");
        if (SafeTag(tag) && NewerVersion(tag, APP_VERSION)) {
            bool fresh = tag != g_updTag;
            g_updTag = tag;
            if (j->kind == NET_UPDATE_MANUAL) OfferUpdate();
            else if (fresh) {
                g_balloonUrl.clear();
                Balloon(L"Update available: WinClock " + tag, L"You have " APP_VERSION L". Click here to update. It takes a few seconds.");
                g_balloonUpdate = true;
            }
        } else if (j->kind == NET_UPDATE_MANUAL) {
            if (tag.empty()) MessageBoxW(nullptr, L"Couldn't check for updates right now. Please try again later.", L"WinClock", MB_OK | MB_ICONINFORMATION);
            else MessageBoxW(nullptr, L"You're up to date (WinClock " APP_VERSION L").", L"WinClock", MB_OK | MB_ICONINFORMATION);
        }
    }
    delete j;
}
static void TravelAddHome();   // features.inc
static void OpenBalloonLink() {
    if (g_balloonUpdate) { g_balloonUpdate = false; OfferUpdate(); return; }
    if (g_balloonTravel) { g_balloonTravel = false; TravelAddHome(); return; }
    std::wstring u = g_balloonUrl; g_balloonUrl.clear();
    if (u.compare(0, 8, L"https://") == 0) ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
