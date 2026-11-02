// state of a time conversion shown on the clocks (clipboard / palette)
struct ConvState { bool on = false; std::wstring line, desc; };
static ConvState g_conv;

// ---------------------------------------------------------------- small reusable dialogs
struct InputDlg { std::wstring title, prompt, value; };
static INT_PTR CALLBACK InputDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    InputDlg* st = (InputDlg*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG:
        st = (InputDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        SetWindowTextW(d, st->title.c_str()); SetDlgItemTextW(d, IDC_IPROMPT, st->prompt.c_str()); SetDlgItemTextW(d, IDC_IEDIT, st->value.c_str());
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDC_IEDIT)); SendDlgItemMessageW(d, IDC_IEDIT, EM_SETSEL, 0, -1);
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) { wchar_t b[2048]; GetDlgItemTextW(d, IDC_IEDIT, b, 2048); st->value = b; EndDialog(d, IDOK); return TRUE; }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(d, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}
static bool InputBox(const std::wstring& title, const std::wstring& prompt, std::wstring& value) {
    InputDlg st{ title, prompt, value };
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_INPUT), g_main, InputDlgProc, (LPARAM)&st) != IDOK) return false;
    value = st.value;
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    return true;
}

static HFONT g_monoFont = nullptr;
static HFONT MonoFont(HWND d) {
    if (!g_monoFont) {
        int h = -MulDiv(10, (int)DpiOf(d), 72);
        g_monoFont = CreateFontW(h, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Cascadia Mono");
    }
    return g_monoFont;
}
static void CopyText(const std::wstring& s) {
    if (!OpenClipboard(g_main)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (h) { memcpy(GlobalLock(h), s.c_str(), (s.size() + 1) * sizeof(wchar_t)); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
    CloseClipboard();
}
static std::wstring ClipboardText() {
    std::wstring s;
    if (!OpenClipboard(g_main)) return s;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) { s.assign(p, wcsnlen(p, 4096)); GlobalUnlock(h); }
    }
    CloseClipboard();
    return s;
}
static std::wstring CRLF(std::wstring s) {   // edit controls want \r\n
    std::wstring o; for (wchar_t c : s) { if (c == L'\n') o += L"\r\n"; else o += c; } return o;
}

struct ReportDlg { std::wstring title, text; };
static INT_PTR CALLBACK ReportDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    ReportDlg* st = (ReportDlg*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG:
        st = (ReportDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        SetWindowTextW(d, st->title.c_str());
        SendDlgItemMessageW(d, IDC_RTEXT, WM_SETFONT, (WPARAM)MonoFont(d), TRUE);
        SetDlgItemTextW(d, IDC_RTEXT, CRLF(st->text).c_str());
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDCANCEL));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_RCOPY) { CopyText(CRLF(st->text)); SetDlgItemTextW(d, IDC_RCOPY, L"Copied ✓"); return TRUE; }
        if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) { EndDialog(d, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}
static void ShowReport(const std::wstring& title, const std::wstring& text) {
    static bool open = false; if (open) return; open = true;
    ReportDlg st{ title, text };
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_REPORT), g_main, ReportDlgProc, (LPARAM)&st);
    open = false;
}

static std::wstring PadRight(std::wstring s, size_t n) { if (s.size() > n) { s.resize(n - 1); s += L"…"; } while (s.size() < n) s += L' '; return s; }
static std::wstring PadLeft(std::wstring s, size_t n) { while (s.size() < n) s = L' ' + s; return s; }

// how inconvenient a time is for one person: 0 fine, 1 early/late, 2 day off / holiday, 3 night
static int PainOf(const Clock& c, const SYSTEMTIME& st, std::wstring* why = nullptr) {
    std::wstring w; int status = ClockStatusAt(c, st, &w);
    switch (status) {
    case ST_WORK: if (why) why->clear(); return 0;
    case ST_HOLIDAY: if (why) *why = w; return 2;
    case ST_AWAY: if (why) *why = L"away"; return 2;
    case ST_SLEEP: if (why) *why = L"night"; return 4;
    default:
        if ((c.offMask >> st.wDayOfWeek) & 1) { if (why) *why = L"day off"; return 2; }
        if (why) *why = st.wHour < WorkStartOf(c) ? L"early" : L"late";
        return 1;
    }
}

// ---------------------------------------------------------------- pinned moments
struct Pin { std::wstring label; long long utc; };
static std::vector<Pin> g_pins;
static void SavePins() {
    WritePrivateProfileStringW(L"Pins", nullptr, nullptr, g_ini.c_str());
    IniPutI(L"Pins", L"Count", (int)g_pins.size());
    for (size_t i = 0; i < g_pins.size(); i++) {
        std::wstring lab = g_pins[i].label; std::replace(lab.begin(), lab.end(), L'|', L'/');
        IniPut(L"Pins", (L"Pin" + std::to_wstring(i + 1)).c_str(), std::to_wstring(g_pins[i].utc) + L"|" + lab);
    }
}
static void LoadPins() {
    g_pins.clear(); int n = IniInt(L"Pins", L"Count", 0);
    for (int i = 1; i <= std::min(n, 20); i++) {
        std::wstring v = IniStr(L"Pins", (L"Pin" + std::to_wstring(i)).c_str(), L"");
        size_t p = v.find(L'|'); if (p == std::wstring::npos) continue;
        g_pins.push_back({ v.substr(p + 1), _wtoi64(v.substr(0, p).c_str()) });
    }
}
static bool PrunePins() {   // a pinned moment disappears a day after it happened
    long long now = NowMin(); size_t before = g_pins.size();
    g_pins.erase(std::remove_if(g_pins.begin(), g_pins.end(), [&](const Pin& p) { return p.utc < now - 1440; }), g_pins.end());
    if (g_pins.size() != before) { SavePins(); return true; }
    return false;
}
static std::wstring PinCountdown(const Pin& p) {
    long long d = p.utc - NowMin();
    if (d > 0) return L"in " + FmtSpan(d);
    if (d > -60) return L"now";
    return FmtSpan(-d) + L" ago";
}
static std::wstring PinTooltip(int k) {
    const Pin& p = g_pins[k];
    std::wstring s = p.label + L"  ·  " + PinCountdown(p) + L"\n";
    CTime lt = ClockAt(g_localClock, (ULONGLONG)p.utc * TPM);
    s += Fmt(L"\nYour time  %ls %d %ls, %ls", kDays[lt.st.wDayOfWeek], lt.st.wDay, kMonths[lt.st.wMonth - 1], FmtHMFull(lt.st).c_str());
    for (size_t i = 0; i < g_clocks.size(); i++) {
        CTime t = ClockAt(g_clocks[i], (ULONGLONG)p.utc * TPM);
        s += L"\n" + DisplayName((int)i) + L"  " + kDays[t.st.wDayOfWeek] + L" " + FmtHMFull(t.st);
    }
    return s + L"\n\nRight-click → Plan → Pinned moments to copy or remove";
}
static void PinNow() {
    std::wstring label = L"Launch";
    if (!InputBox(L"Pin this moment", L"Name for this moment (it stays on the widget and counts down in every zone):", label) || label.empty()) return;
    g_pins.push_back({ label, NowMin() + g_shift });
    std::sort(g_pins.begin(), g_pins.end(), [](const Pin& a, const Pin& b) { return a.utc < b.utc; });
    if (g_pins.size() > 20) g_pins.erase(g_pins.begin());
    SavePins();
}

// ---------------------------------------------------------------- meeting card + .ics
static std::wstring MeetingCard(const std::wstring& title, long long utc, int dur) {
    CTime lt = ClockAt(g_localClock, (ULONGLONG)utc * TPM);
    std::wstring s = title + L"  —  " + Fmt(L"%ls %d %ls %d", kDays[lt.st.wDayOfWeek], lt.st.wDay, kMonths[lt.st.wMonth - 1], lt.st.wYear) +
                     Fmt(L", %d min\n", dur);
    size_t w = 10; for (size_t i = 0; i < g_clocks.size(); i++) w = std::max(w, DisplayName((int)i).size() + 2);
    w = std::min<size_t>(w, 28);
    for (size_t i = 0; i < g_clocks.size(); i++) {
        CTime a = ClockAt(g_clocks[i], (ULONGLONG)utc * TPM), b = ClockAt(g_clocks[i], (ULONGLONG)(utc + dur) * TPM);
        std::wstring why; PainOf(g_clocks[i], a.st, &why);
        s += L"  " + PadRight(DisplayName((int)i), w) + PadLeft(FmtHMFull(a.st), S.h24 ? 5 : 8) + L" \u2013 " +
             PadLeft(FmtHMFull(b.st), S.h24 ? 5 : 8) + L"  " + kDays[a.st.wDayOfWeek] + (why.empty() ? L"" : L"   (" + why + L")") + L"\n";
    }
    return s;
}
static std::wstring IcsEscape(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) { if (c == L'\\' || c == L';' || c == L',') { o += L'\\'; o += c; } else if (c == L'\n') o += L"\\n"; else if (c != L'\r') o += c; }
    return o;
}
static std::string Utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
static std::wstring FromUtf8(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
static std::wstring IcsStamp(long long utc) { SYSTEMTIME st = StOfMin(utc); return Fmt(L"%04d%02d%02dT%02d%02d00Z", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); }
static bool SaveIcs(HWND owner, const std::wstring& title, long long utc, int dur) {
    wchar_t file[MAX_PATH]; std::wstring def = title;
    for (auto& c : def) if (wcschr(L"\\/:*?\"<>|", c)) c = L'-';
    wcsncpy(file, (def + L".ics").c_str(), MAX_PATH - 1); file[MAX_PATH - 1] = 0;
    OPENFILENAMEW of{ sizeof(of) }; of.hwndOwner = owner; of.lpstrFilter = L"Calendar invite (*.ics)\0*.ics\0"; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
    of.lpstrDefExt = L"ics"; of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&of)) return false;
    std::wstring card = MeetingCard(title, utc, dur);
    std::wstring ics = L"BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//WinClock//EN\r\nCALSCALE:GREGORIAN\r\nMETHOD:PUBLISH\r\nBEGIN:VEVENT\r\n";
    ics += Fmt(L"UID:%lld-%u@winclock\r\n", utc, GetTickCount());
    ics += L"DTSTAMP:" + IcsStamp(NowMin()) + L"\r\nDTSTART:" + IcsStamp(utc) + L"\r\nDTEND:" + IcsStamp(utc + dur) + L"\r\n";
    ics += L"SUMMARY:" + IcsEscape(title) + L"\r\nDESCRIPTION:" + IcsEscape(card) + L"\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string u = Utf8(ics); DWORD wr; WriteFile(h, u.data(), (DWORD)u.size(), &wr, nullptr); CloseHandle(h);
    return true;
}
struct CardDlg { long long utc; };
static void CardRefresh(HWND d, CardDlg* st) {
    wchar_t t[256]; GetDlgItemTextW(d, IDC_CTITLE, t, 256);
    static const int durs[] = { 15, 30, 45, 60, 90, 120 };
    int dur = durs[std::clamp((int)SendDlgItemMessageW(d, IDC_CDUR, CB_GETCURSEL, 0, 0), 0, 5)];
    SetDlgItemTextW(d, IDC_CTEXT, CRLF(MeetingCard(t[0] ? t : L"Meeting", st->utc, dur)).c_str());
}
static INT_PTR CALLBACK CardDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    CardDlg* st = (CardDlg*)GetWindowLongPtrW(d, DWLP_USER);
    static const int durs[] = { 15, 30, 45, 60, 90, 120 };
    switch (m) {
    case WM_INITDIALOG:
        st = (CardDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        SetDlgItemTextW(d, IDC_CTITLE, L"Meeting");
        for (int x : durs) SendDlgItemMessageW(d, IDC_CDUR, CB_ADDSTRING, 0, (LPARAM)Fmt(L"%d minutes", x).c_str());
        SendDlgItemMessageW(d, IDC_CDUR, CB_SETCURSEL, 1, 0);
        SendDlgItemMessageW(d, IDC_CTEXT, WM_SETFONT, (WPARAM)MonoFont(d), TRUE);
        CardRefresh(d, st);
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDC_CTITLE)); SendDlgItemMessageW(d, IDC_CTITLE, EM_SETSEL, 0, -1);
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_CTITLE: if (HIWORD(wp) == EN_CHANGE) CardRefresh(d, st); break;
        case IDC_CDUR: if (HIWORD(wp) == CBN_SELCHANGE) CardRefresh(d, st); break;
        case IDC_CCOPY: {
            int n = GetWindowTextLengthW(GetDlgItem(d, IDC_CTEXT)); std::wstring t(n + 1, L'\0');
            GetDlgItemTextW(d, IDC_CTEXT, &t[0], n + 1); t.resize(n); CopyText(t);
            SetDlgItemTextW(d, IDC_CCOPY, L"Copied ✓"); return TRUE;
        }
        case IDC_CICS: {
            wchar_t t[256]; GetDlgItemTextW(d, IDC_CTITLE, t, 256);
            int dur = durs[std::clamp((int)SendDlgItemMessageW(d, IDC_CDUR, CB_GETCURSEL, 0, 0), 0, 5)];
            if (SaveIcs(d, t[0] ? t : L"Meeting", st->utc, dur)) SetDlgItemTextW(d, IDC_CICS, L"Saved ✓");
            return TRUE;
        }
        case IDCANCEL: case IDOK: EndDialog(d, IDCANCEL); return TRUE;
        }
        break;
    }
    return FALSE;
}
static void MeetingCardDialog() {
    static bool open = false; if (open) return; open = true;
    CardDlg st{ NowMin() + g_shift };
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_CARD), g_main, CardDlgProc, (LPARAM)&st);
    open = false; TrimMemory();
}

// ---------------------------------------------------------------- fairness meter
static std::wstring FairnessReport() {
    std::wstring s;
    long long now = NowMin();
    int meetings = 0;
    for (auto& a0 : g_alarms) {
        if (!a0.enabled || a0.rep == 0 || g_clocks.empty()) continue;
        meetings++;
        Alarm a = a0; long long after = now; int occ = 0;
        std::vector<int> bad(g_clocks.size(), 0), pain(g_clocks.size(), 0);
        std::vector<std::wstring> kind(g_clocks.size());
        std::vector<SYSTEMTIME> sample(g_clocks.size());
        for (int k = 0; k < 40; k++) {
            ComputeNext(a, after); if (a.next < 0 || a.next > now + 28 * 1440) break;
            occ++;
            for (size_t i = 0; i < g_clocks.size(); i++) {
                SYSTEMTIME st = ClockAt(g_clocks[i], (ULONGLONG)a.next * TPM).st; std::wstring why;
                int p = PainOf(g_clocks[i], st, &why);
                if (k == 0) sample[i] = st;
                if (p) { bad[i]++; pain[i] += p; if (kind[i].empty()) kind[i] = why; }
            }
            after = a.next;
        }
        if (!occ) continue;
        s += a.label + L"  —  " + AlarmWhen(a0, false) + L", " + Lower(kRepeat[a.rep]) + Fmt(L"  (%d meetings in the next 4 weeks)\n", occ);
        size_t w = 12; for (size_t i = 0; i < g_clocks.size(); i++) w = std::max(w, DisplayName((int)i).size() + 2);
        w = std::min<size_t>(w, 26);
        int worst = -1;
        for (size_t i = 0; i < g_clocks.size(); i++) {
            int pct = bad[i] * 100 / occ;
            std::wstring bar; for (int q = 0; q < 10; q++) bar += q < (pct + 5) / 10 ? L"█" : L"·";
            s += L"  " + PadRight(DisplayName((int)i), w) + bar + Fmt(L" %3d%% outside hours", pct) +
                 (bad[i] ? L"  (" + kind[i] + L", e.g. " + Lower(FmtHMFull(sample[i])) + L")" : L"") + L"\n";
            if (bad[i] && kind[i] != L"away" && (worst < 0 || pain[i] > pain[worst])) worst = (int)i;
        }
        if (worst >= 0) s += L"  → " + DisplayName(worst) + Fmt(L" carries most of the inconvenience (%d%% of meetings).\n", bad[worst] * 100 / occ);
        else s += L"  → Fair: this time is inside everyone's working hours.\n";

        // suggestion: best single time (quarter-hour steps, in the meeting's zone) or a weekly rotation
        Clock z = ZoneClock(a0.tzid);
        ComputeNext(a, now); long long base = a.next;
        if (worst >= 0 && base > 0) {
            SYSTEMTIME day = ClockAt(z, (ULONGLONG)base * TPM).st;
            struct Slot { int h, m; long long utc; std::vector<int> p; int mx = 0, sum = 0; };
            std::vector<Slot> slots;
            for (int q = 0; q < 96; q++) {
                SYSTEMTIME lt = day; lt.wHour = (WORD)(q / 4); lt.wMinute = (WORD)(q % 4 * 15); long long u;
                if (!LocalToUtcMin(z, lt, u)) continue;
                if (q / 4 < 7 || q / 4 >= 22) continue;   // never suggest the organiser's own night
                Slot sl; sl.h = q / 4; sl.m = q % 4 * 15; sl.utc = u;
                for (auto& c : g_clocks) {
                    SYSTEMTIME cst = ClockAt(c, (ULONGLONG)u * TPM).st;
                    int st = ClockStatusAt(c, cst);
                    int p = (st == ST_AWAY || st == ST_HOLIDAY) ? 0 : PainOf(c, cst);   // absent people don't vote
                    sl.p.push_back(p); sl.mx = std::max(sl.mx, p); sl.sum += p;
                }
                slots.push_back(sl);
            }
            auto fmtSlot = [&](const Slot& sl) { SYSTEMTIME t{}; t.wHour = (WORD)sl.h; t.wMinute = (WORD)sl.m; return Lower(FmtHMFull(t)) + L" " + PlaceName(a0.tzid); };
            const Slot* best = nullptr;
            for (auto& sl : slots) if (!best || sl.mx < best->mx || (sl.mx == best->mx && sl.sum < best->sum)) best = &sl;
            if (best && best->mx == 0) s += L"  ✓ Suggestion: move it to " + fmtSlot(*best) + L" — inside working hours for everyone.\n";
            else if (best) {
                const Slot* ra = nullptr; const Slot* rb = nullptr; double bestScore = 1e9;
                for (size_t i = 0; i < slots.size(); i++) for (size_t j = i + 1; j < slots.size(); j++) {
                    double mx = 0, sum = 0;
                    for (size_t c = 0; c < g_clocks.size(); c++) { double v = (slots[i].p[c] + slots[j].p[c]) / 2.0; mx = std::max(mx, v); sum += v; }
                    double score = mx * 100 + sum;
                    if (score < bestScore) { bestScore = score; ra = &slots[i]; rb = &slots[j]; }
                }
                if (ra && rb && bestScore < best->mx * 100 + best->sum)
                    s += L"  ↻ Suggestion: no single time works. Alternate weekly between " + fmtSlot(*ra) + L" and " + fmtSlot(*rb) +
                         L" so the early/late calls are shared.\n";
                else s += L"  ✓ Least painful single time: " + fmtSlot(*best) + L".\n";
            }
        }
        s += L"\n";
    }
    if (!meetings)
        s = L"No recurring meetings yet.\n\nAdd one with Alarms & countdowns → Add (set Repeat to weekdays or every day),\n"
            L"and WinClock will show who keeps getting the early, late or night calls and suggest\na fairer time or a rotation.\n";
    else s += L"Working hours and days off come from each clock (Clocks → a clock → Person, hours & holidays).\n"
              L"Night = 23:00–07:00. Holidays and away days count as outside hours.";
    return s;
}

// ---------------------------------------------------------------- daylight-saving radar
struct DstEvt { std::wstring who, tzid; long long at; int oldOff, newOff; };
static std::vector<DstEvt> g_dst; static long long g_dstAt = -1;
static int OffsetAt(const Clock& c, long long u) { return ClockAt(c, (ULONGLONG)u * TPM).offset; }
static bool NextTransition(const Clock& c, long long from, int days, long long& at, int& o, int& n) {
    int prev = OffsetAt(c, from);
    for (long long t = from + 360; t <= from + days * 1440LL; t += 360) {
        int cur = OffsetAt(c, t);
        if (cur != prev) {
            long long lo = t - 360, hi = t;
            while (hi - lo > 1) { long long mid = (lo + hi) / 2; if (OffsetAt(c, mid) == prev) lo = mid; else hi = mid; }
            at = hi; o = prev; n = cur; return true;
        }
    }
    return false;
}
static void ComputeDst(bool force = false) {
    long long now = NowMin();
    if (!force && g_dstAt >= 0 && now - g_dstAt < 60) return;
    g_dstAt = now; g_dst.clear();
    std::vector<std::pair<std::wstring, Clock>> zones = { { L"Your time", g_localClock } };
    for (size_t i = 0; i < g_clocks.size(); i++) zones.push_back({ DisplayName((int)i), g_clocks[i] });
    std::vector<std::wstring> seen;
    for (auto& z : zones) {
        std::wstring key = z.second.kind == 1 ? L"Local" : z.second.tzid;
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
        seen.push_back(key);
        long long at; int o, n;
        if (NextTransition(z.second, now, 60, at, o, n)) g_dst.push_back({ z.first, key, at, o, n });
    }
    std::sort(g_dst.begin(), g_dst.end(), [](const DstEvt& a, const DstEvt& b) { return a.at < b.at; });
}
static std::wstring DstWhen(const DstEvt& e) {
    SYSTEMTIME lt = StOfMin(e.at + e.oldOff);
    return Fmt(L"%ls %d %ls", kDays[lt.wDayOfWeek], lt.wDay, kMonths[lt.wMonth - 1]);
}
static std::wstring DstVerb(const DstEvt& e) {
    int d = std::abs(e.newOff - e.oldOff);
    std::wstring amt = d % 60 ? Fmt(L"%dm", d) : Fmt(L"%dh", d / 60);
    return (e.newOff < e.oldOff ? L"go back " : L"go forward ") + amt;
}
static std::wstring DstNoteFor(const Clock& c) {
    ComputeDst();
    std::wstring key = c.kind == 1 ? L"Local" : c.tzid;
    for (auto& e : g_dst) if (e.tzid == key && e.at - NowMin() <= 30 * 1440) return L"Clocks " + DstVerb(e) + L" on " + DstWhen(e);
    return L"";
}
static std::wstring DstShort() {   // for the status bar, within 7 days
    ComputeDst();
    for (auto& e : g_dst) if (e.at - NowMin() <= 7 * 1440 && e.at > NowMin())
        return L"dst: " + e.who + L" " + (e.newOff < e.oldOff ? L"−1h " : L"+1h ") + Lower(DstWhen(e).substr(0, 3));
    return L"";
}
static std::wstring RelText(int mins) {   // "5h30 behind you"
    if (mins == 0) return L"the same as you";
    int a = std::abs(mins);
    std::wstring v = a % 60 ? Fmt(L"%dh%02d", a / 60, a % 60) : Fmt(L"%dh", a / 60);
    return v + (mins < 0 ? L" behind you" : L" ahead of you");
}
static std::wstring DstImpact(const DstEvt& e) {
    std::wstring s;
    if (e.tzid != L"Local") {
        int lb = OffsetAt(g_localClock, e.at - 1), la = OffsetAt(g_localClock, e.at + 1);
        s += L"    " + e.who + L" will be " + RelText(e.newOff - la) + L" (now " + RelText(e.oldOff - lb) + L").\n";
    }
    // alarms whose "your time" moves because of this change
    for (auto& a : g_alarms) {
        if (!a.enabled) continue;
        Clock z = ZoneClock(a.tzid);
        bool zoneMoves = (a.tzid == e.tzid) || (e.tzid == L"Local" && z.kind != 1);
        if (!zoneMoves && !(e.tzid != L"Local" && z.kind == 1)) continue;
        long long ub, ua; SYSTEMTIME d1 = StOfMin(e.at - 2 * 1440 + OffsetAt(z, e.at)), d2 = StOfMin(e.at + 2 * 1440 + OffsetAt(z, e.at));
        d1.wHour = d2.wHour = (WORD)a.hh; d1.wMinute = d2.wMinute = (WORD)a.mm;
        if (!LocalToUtcMin(z, d1, ub) || !LocalToUtcMin(z, d2, ua)) continue;
        if (z.kind == 1) {   // alarm set in your time: the other side moves
            Clock other = ZoneClock(e.tzid);
            SYSTEMTIME b = ClockAt(other, (ULONGLONG)ub * TPM).st, af = ClockAt(other, (ULONGLONG)ua * TPM).st;
            if (b.wHour != af.wHour || b.wMinute != af.wMinute)
                s += L"    • \"" + a.label + L"\" (" + Lower(FmtHMFull(b)) + L" your time) will be " + Lower(FmtHMFull(af)) + L" for " + e.who + L" instead of " + Lower(FmtHMFull(b)) + L".\n";
        } else {
            SYSTEMTIME b = ClockAt(g_localClock, (ULONGLONG)ub * TPM).st, af = ClockAt(g_localClock, (ULONGLONG)ua * TPM).st;
            if (b.wHour != af.wHour || b.wMinute != af.wMinute)
                s += L"    • \"" + a.label + L"\" (" + AlarmWhen(a, false) + L") will be " + Lower(FmtHMFull(af)) + L" your time instead of " + Lower(FmtHMFull(b)) + L".\n";
        }
    }
    return s;
}
static std::wstring DstReport() {
    ComputeDst(true);
    if (g_dst.empty()) return L"No daylight-saving changes in the next 60 days for any of your clocks.";
    std::wstring s = L"Daylight-saving changes in the next 60 days\n\n";
    for (auto& e : g_dst) {
        SYSTEMTIME lt = StOfMin(e.at + e.oldOff);
        s += DstWhen(e) + L"  —  " + e.who + L": clocks " + DstVerb(e) + Fmt(L" at %02d:%02d local", lt.wHour, lt.wMinute) +
             L" (in " + FmtSpan(std::max(1LL, e.at - NowMin())) + L")\n" + DstImpact(e) + L"\n";
    }
    // temporary gaps: two places shift on different dates, so their gap is unusual in between
    for (size_t i = 0; i < g_dst.size(); i++) for (size_t j = i + 1; j < g_dst.size(); j++) {
        const DstEvt& a = g_dst[i]; const DstEvt& b = g_dst[j];
        if ((a.newOff - a.oldOff) != (b.newOff - b.oldOff) || b.at - a.at > 35 * 1440 || b.at - a.at < 1440) continue;
        int usual = std::abs(b.oldOff - a.oldOff), during = std::abs(b.oldOff - a.newOff);
        if (usual == during) continue;
        s += L"Between " + DstWhen(a) + L" and " + DstWhen(b) + L", " + a.who + L" and " + b.who +
             Fmt(L" are %ls apart instead of the usual %ls.\n", FmtSpan(during).c_str(), FmtSpan(usual).c_str());
    }
    return s;
}
static void CheckDstNotify() {
    ComputeDst();
    long long now = NowMin();
    for (auto& e : g_dst) {
        long long left = e.at - now; if (left <= 0) continue;
        const wchar_t* stage = left <= 1440 ? L"1" : left <= 7 * 1440 ? L"7" : nullptr;
        if (!stage) continue;
        std::wstring key = e.tzid + Fmt(L"@%lld#", e.at / 1440) + stage;
        if (IniInt(L"DstNotified", key.c_str(), 0)) continue;
        IniPutI(L"DstNotified", key.c_str(), 1);
        std::wstring body = DstImpact(e);
        for (auto& c : body) if (c == L'•') c = L'-';
        std::wstring t; for (size_t p = 0; p < body.size(); p++) if (!(body[p] == L' ' && (p == 0 || body[p - 1] == L'\n'))) t += body[p];
        Balloon(e.who + L": clocks " + DstVerb(e) + L" " + DstWhen(e), t.empty() ? L"Your clocks in WinClock adjust automatically." : t);
    }
}

// ---------------------------------------------------------------- share codes
static std::wstring ShareCode() {
    std::wstring p = L"WC1";
    for (auto& c : g_clocks) {
        std::wstring f[] = { c.label, c.tzid, c.person, c.ws >= 0 ? std::to_wstring(c.ws) : L"", c.we >= 0 ? std::to_wstring(c.we) : L"",
                             std::to_wstring(c.offMask), c.cc, std::isnan(c.lat) ? L"" : Fmt(L"%.3f", c.lat), std::isnan(c.lon) ? L"" : Fmt(L"%.3f", c.lon) };
        p += L'\x1e';
        for (int k = 0; k < 9; k++) { if (k) p += L'\x1f'; p += f[k]; }
    }
    std::string u = Utf8(p); DWORD n = 0;
    CryptBinaryToStringA((const BYTE*)u.data(), (DWORD)u.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &n);
    std::string b(n, '\0'); CryptBinaryToStringA((const BYTE*)u.data(), (DWORD)u.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &b[0], &n);
    b.resize(strlen(b.c_str()));
    return L"WC1:" + FromUtf8(b);
}
static bool ParseShareCode(std::wstring code, std::vector<Clock>& out) {
    size_t p = code.find(L"WC1:"); if (p == std::wstring::npos) return false;
    code = code.substr(p + 4);
    std::string a; for (wchar_t c : code) { if (iswalnum(c) || c == L'+' || c == L'/' || c == L'=') a += (char)c; else if (!iswspace(c)) break; }
    DWORD n = 0;
    if (!CryptStringToBinaryA(a.c_str(), (DWORD)a.size(), CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr) || n > 65536) return false;
    std::string bin(n, '\0');
    if (!CryptStringToBinaryA(a.c_str(), (DWORD)a.size(), CRYPT_STRING_BASE64, (BYTE*)&bin[0], &n, nullptr, nullptr)) return false;
    bin.resize(n);
    auto recs = Split(FromUtf8(bin), L'\x1e');
    if (recs.empty() || recs[0] != L"WC1") return false;
    for (size_t i = 1; i < recs.size() && out.size() < 50; i++) {
        auto f = Split(recs[i], L'\x1f'); if (f.size() < 2 || f[0].empty()) continue;
        Clock c; c.label = f[0].substr(0, 60); c.tzid = f[1].substr(0, 80);
        if (f.size() > 2) c.person = f[2].substr(0, 40);
        if (f.size() > 4) { c.ws = f[3].empty() ? -1 : std::clamp(_wtoi(f[3].c_str()), 0, 23); c.we = f[4].empty() ? -1 : std::clamp(_wtoi(f[4].c_str()), 1, 24); }
        if (f.size() > 5 && !f[5].empty()) c.offMask = _wtoi(f[5].c_str()) & 0x7F;
        if (f.size() > 6) c.cc = f[6].substr(0, 3);
        if (f.size() > 8 && !f[7].empty() && !f[8].empty()) { c.lat = std::clamp(_wtof(f[7].c_str()), -90.0, 90.0); c.lon = std::clamp(_wtof(f[8].c_str()), -180.0, 180.0); }
        ResolveClock(c);
        if (!c.valid) continue;
        FillFromAlias(c);
        out.push_back(c);
    }
    return !out.empty();
}
static void ImportShareCode() {
    std::wstring code = ClipboardText();
    if (code.find(L"WC1:") == std::wstring::npos) {
        code.clear();
        if (!InputBox(L"Import share code", L"Paste a WinClock share code (it starts with WC1:):", code)) return;
    }
    std::vector<Clock> in;
    if (!ParseShareCode(code, in)) { MessageBoxW(nullptr, L"That doesn't look like a WinClock share code.", L"WinClock", MB_OK | MB_ICONWARNING); return; }
    std::wstring names; for (size_t i = 0; i < in.size() && i < 12; i++) names += (i ? L", " : L"") + DisplayName(in[i]);
    if (in.size() > 12) names += L", …";
    int r = MessageBoxW(nullptr, (Fmt(L"Import %d clocks: ", (int)in.size()) + names + L"?\n\nYes = add them to your clocks\nNo = replace your clocks with them").c_str(),
                        L"WinClock — import", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return;
    if (r == IDNO) g_clocks.clear();
    for (auto& c : in) {
        bool dup = false; for (auto& e : g_clocks) if (e.label == c.label && e.tzid == c.tzid && e.person == c.person) dup = true;
        if (!dup && g_clocks.size() < 50) g_clocks.push_back(c);
    }
    SaveClocks(); ComputeDst(true);
}

// ---------------------------------------------------------------- profiles
static std::vector<std::wstring> ProfileNames() {
    std::vector<std::wstring> v = { L"Default" };
    for (auto& n : Split(IniStr(L"Profiles", L"Names", L""), L'|')) if (!n.empty() && n != L"Default") v.push_back(n);
    return v;
}
static void SaveProfileNames(const std::vector<std::wstring>& v) {
    std::wstring s; for (auto& n : v) if (n != L"Default") s += (s.empty() ? L"" : L"|") + n;
    IniPut(L"Profiles", L"Names", s);
}
static void SwitchProfile(const std::wstring& name) {
    if (name == S.profile) return;
    SaveClocks();
    S.profile = name; SaveSettings();
    LoadClocks(); ComputeDst(true);
}
static void NewProfile() {
    std::wstring name = ProfileNames().size() == 1 ? L"Work" : L"Personal";
    if (!InputBox(L"New profile", L"Name for the new profile (it starts as a copy of your current clocks).\nTip: name two profiles \"Work\" and \"Personal\" to switch automatically.", name) || name.empty()) return;
    for (auto& c : name) if (c == L'|' || c == L'[' || c == L']' || c == L'=') c = L'-';
    auto names = ProfileNames();
    if (std::find(names.begin(), names.end(), name) == names.end()) { names.push_back(name); SaveProfileNames(names); }
    SaveClocks();
    S.profile = name; SaveClocks(); SaveSettings();
}
static void DeleteProfile() {
    if (S.profile == L"Default") return;
    if (MessageBoxW(nullptr, (L"Delete the profile \"" + S.profile + L"\" and its clocks?").c_str(), L"WinClock", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    WritePrivateProfileStringW(ProfileSection().c_str(), nullptr, nullptr, g_ini.c_str());
    auto names = ProfileNames(); names.erase(std::remove(names.begin(), names.end(), S.profile), names.end()); SaveProfileNames(names);
    S.profile = L"Default"; SaveSettings(); LoadClocks();
}
static bool AutoProfileTick() {   // Work during your working hours on weekdays, Personal otherwise
    if (!S.autoProfile) return false;
    auto names = ProfileNames();
    bool hasW = std::find(names.begin(), names.end(), L"Work") != names.end(), hasP = std::find(names.begin(), names.end(), L"Personal") != names.end();
    if (!hasW || !hasP) return false;
    SYSTEMTIME lt; GetLocalTime(&lt);
    bool work = lt.wDayOfWeek != 0 && lt.wDayOfWeek != 6 && lt.wHour >= S.workStart && lt.wHour < S.workEnd;
    std::wstring want = work ? L"Work" : L"Personal";
    if (want == S.profile) return false;
    SwitchProfile(want); return true;
}

// ---------------------------------------------------------------- person, hours & holidays dialog
struct PersonDlg { Clock c; };
static INT_PTR CALLBACK PersonDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    PersonDlg* st = (PersonDlg*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG: {
        st = (PersonDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        Clock& c = st->c;
        SetWindowTextW(d, (L"Person, hours & holidays — " + c.label).c_str());
        SetDlgItemTextW(d, IDC_PNAME, c.person.c_str());
        SendDlgItemMessageW(d, IDC_PSTART, CB_ADDSTRING, 0, (LPARAM)Fmt(L"Default (%02d:00)", S.workStart).c_str());
        SendDlgItemMessageW(d, IDC_PEND, CB_ADDSTRING, 0, (LPARAM)Fmt(L"Default (%02d:00)", S.workEnd).c_str());
        for (int h = 0; h < 24; h++) SendDlgItemMessageW(d, IDC_PSTART, CB_ADDSTRING, 0, (LPARAM)Fmt(L"%02d:00", h).c_str());
        for (int h = 1; h <= 24; h++) SendDlgItemMessageW(d, IDC_PEND, CB_ADDSTRING, 0, (LPARAM)Fmt(L"%02d:00", h % 24).c_str());
        SendDlgItemMessageW(d, IDC_PSTART, CB_SETCURSEL, c.ws < 0 ? 0 : c.ws + 1, 0);
        SendDlgItemMessageW(d, IDC_PEND, CB_SETCURSEL, c.we < 0 ? 0 : c.we, 0);
        for (int k = 0; k < 7; k++) CheckDlgButton(d, IDC_POFF0 + k, (c.offMask >> k) & 1 ? BST_CHECKED : BST_UNCHECKED);
        std::wstring autoCc = c.kind == 1 ? CountryForZone(g_localKey) : CountryForZone(c.tzid);
        SendDlgItemMessageW(d, IDC_PCC, CB_ADDSTRING, 0, (LPARAM)(L"Automatic" + (autoCc.empty() ? std::wstring(L" (none)") : L" (" + std::wstring(CountryName(autoCc)) + L")")).c_str());
        SendDlgItemMessageW(d, IDC_PCC, CB_ADDSTRING, 0, (LPARAM)L"No holidays");
        int sel = c.cc.empty() ? 0 : c.cc == L"-" ? 1 : 0;
        for (size_t k = 0; k < sizeof(kCountries) / sizeof(*kCountries); k++) {
            SendDlgItemMessageW(d, IDC_PCC, CB_ADDSTRING, 0, (LPARAM)kCountries[k].name);
            if (_wcsicmp(c.cc.c_str(), kCountries[k].cc) == 0) sel = (int)k + 2;
        }
        SendDlgItemMessageW(d, IDC_PCC, CB_SETCURSEL, sel, 0);
        SetDlgItemTextW(d, IDC_PNOTE, c.note.c_str());
        if (c.awayY) SetDlgItemTextW(d, IDC_PAWAY, Fmt(L"%04d-%02d-%02d", c.awayY, c.awayM, c.awayD).c_str());
        CheckDlgButton(d, IDC_PTRAY, c.tray ? BST_CHECKED : BST_UNCHECKED);
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDC_PNAME));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            Clock& c = st->c; wchar_t b[256];
            GetDlgItemTextW(d, IDC_PNAME, b, 256); c.person = b;
            int a = (int)SendDlgItemMessageW(d, IDC_PSTART, CB_GETCURSEL, 0, 0), e = (int)SendDlgItemMessageW(d, IDC_PEND, CB_GETCURSEL, 0, 0);
            c.ws = a <= 0 ? -1 : a - 1; c.we = e <= 0 ? -1 : e;
            c.offMask = 0; for (int k = 0; k < 7; k++) if (IsDlgButtonChecked(d, IDC_POFF0 + k) == BST_CHECKED) c.offMask |= 1 << k;
            int cc = (int)SendDlgItemMessageW(d, IDC_PCC, CB_GETCURSEL, 0, 0);
            c.cc = cc <= 0 ? L"" : cc == 1 ? L"-" : kCountries[cc - 2].cc;
            GetDlgItemTextW(d, IDC_PNOTE, b, 256); c.note = b;
            GetDlgItemTextW(d, IDC_PAWAY, b, 256); std::wstring aw = b;
            c.awayY = c.awayM = c.awayD = 0;
            if (aw.find_first_not_of(L' ') != std::wstring::npos && !ParseDate(aw, c.awayY, c.awayM, c.awayD)) {
                MessageBoxW(d, L"Type the \"away until\" date as YYYY-MM-DD, or leave it empty.", L"WinClock", MB_OK | MB_ICONWARNING); return TRUE;
            }
            c.tray = IsDlgButtonChecked(d, IDC_PTRAY) == BST_CHECKED;
            EndDialog(d, IDOK); return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(d, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}
static void PersonDialog(int idx) {
    static bool open = false; if (open || idx < 0 || idx >= (int)g_clocks.size()) return; open = true;
    PersonDlg st{ g_clocks[idx] };
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_PERSON), g_main, PersonDlgProc, (LPARAM)&st) == IDOK && idx < (int)g_clocks.size()) {
        if (st.c.tray) for (auto& c : g_clocks) c.tray = false;
        g_clocks[idx] = st.c; SaveClocks(); ClearHolidayCache();
        if (st.c.tray) S.trayTime = true;
        SaveSettings();
    }
    open = false; TrimMemory();
}
