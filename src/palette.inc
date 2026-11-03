// ---------------------------------------------------------------- command palette (Ctrl+Alt+K by default)
// Type "tokyo", "add berlin", "3pm ny thu", "meeting", "theme paper"... Enter runs the highlighted line.
struct PalItem { std::wstring title, sub; std::function<void()> run; bool keepOpen = false; bool copied = false; };
static HWND g_pal = nullptr, g_palEdit = nullptr;
static std::vector<PalItem> g_palItems; static int g_palSel = 0;
static HFONT g_palFont = nullptr; static HBRUSH g_palBrush = nullptr;
static std::vector<ZoneEntry> g_palZones;
static void ConvertTo(const WhenResult& w);   // defined below
static void ShowFlyoutAlways();
static void HandleCommand(int id);

static void PalClose() {
    if (!g_pal) return;
    HWND h = g_pal; g_pal = nullptr; g_palEdit = nullptr;
    DestroyWindow(h);
    g_palItems.clear(); g_palZones.clear(); g_palZones.shrink_to_fit();
    TrimMemory();
}
static void PalAddClocks(const std::vector<std::pair<std::wstring, std::wstring>>& picks) {
    for (auto& pk : picks) {
        if (g_clocks.size() >= 50) break;
        Clock c; c.label = pk.first; c.tzid = pk.second; ResolveClock(c); FillFromAlias(c);
        if (c.valid || c.kind) g_clocks.push_back(c);
    }
    SaveClocks(); ComputeDst(true); RefreshAll();
}
static void PalBuild(const std::wstring& raw) {
    g_palItems.clear(); g_palSel = 0;
    std::wstring q = raw; while (!q.empty() && iswspace(q.back())) q.pop_back(); while (!q.empty() && iswspace(q.front())) q.erase(q.begin());
    std::wstring t = Lower(q);
    auto add = [&](std::wstring title, std::wstring sub, std::function<void()> fn, bool keep = false) {
        if (g_palItems.size() < 9) g_palItems.push_back({ title, sub, fn, keep });
    };
    auto hint = [&](const wchar_t* text, const wchar_t* sub) { std::wstring tx = text; add(tx, sub, [tx]() { SetWindowTextW(g_palEdit, tx.c_str()); SendMessageW(g_palEdit, EM_SETSEL, tx.size(), tx.size()); }, true); };
    if (t.empty()) {
        hint(L"9pm india to germany", L"time calculator: any time, any two places");
        hint(L"3pm ny thu", L"convert a time into every clock");
        hint(L"add berlin", L"add a city (or a whole country: add australia)");
        hint(L"tokyo", L"what time is it there?");
        hint(L"meeting", L"find the next slot when everyone is working");
        hint(L"card", L"copy a meeting card or save an .ics invite");
        hint(L"pin launch", L"pin the current (or planned) moment");
        hint(L"fair", L"who keeps getting the bad hours?");
        hint(L"dst", L"upcoming daylight-saving changes");
        hint(L"settings", L"theme, layout, taskbar, calendar…");
        return;
    }
    auto starts = [&](const wchar_t* p) { size_t n = wcslen(p); return t.compare(0, n, p) == 0 ? t.substr(n) : std::wstring(L"\x01"); };
    // add <place>
    std::wstring arg = starts(L"add ");
    if (arg != L"\x01") {
        if (g_palZones.empty()) g_palZones = BuildZoneEntries();
        for (auto& z : g_palZones) {
            std::wstring hay = Lower(z.text + L" " + z.tzid);
            if (arg.empty() || hay.find(arg) == std::wstring::npos) continue;
            std::vector<std::pair<std::wstring, std::wstring>> picks;
            if (z.group.empty()) picks.push_back({ z.city, z.tzid }); else picks = z.group;
            add(L"Add " + (z.group.empty() ? z.city : z.city + Fmt(L" (%d zones)", (int)z.group.size())), z.text, [picks]() { PalAddClocks(picks); });
        }
        if (g_palItems.empty()) add(L"No place matches “" + arg + L"”", L"try a city, country or zone name", nullptr, true);
        return;
    }
    for (const wchar_t* pre : { L"remove ", L"rm ", L"delete " }) {
        arg = starts(pre); if (arg == L"\x01") continue;
        for (size_t i = 0; i < g_clocks.size(); i++) if (Lower(DisplayName((int)i)).find(arg) != std::wstring::npos) {
            std::wstring name = DisplayName((int)i), tz = g_clocks[i].tzid, lab = g_clocks[i].label;
            add(L"Remove " + name, g_clocks[i].tzid, [lab, tz]() {
                for (size_t k = 0; k < g_clocks.size(); k++) if (g_clocks[k].label == lab && g_clocks[k].tzid == tz) { g_clocks.erase(g_clocks.begin() + k); break; }
                SaveClocks(); RefreshAll(); });
        }
        return;
    }
    arg = starts(L"theme ");
    if (arg != L"\x01") { for (int i = 0; i < T_COUNT; i++) { std::wstring n = kThemeName[i]; n.erase(std::remove(n.begin(), n.end(), L'&'), n.end()); if (Lower(n).find(arg) != std::wstring::npos) add(L"Theme: " + n, L"", [i]() { HandleCommand(120 + i); }); } return; }
    arg = starts(L"layout ");
    if (arg != L"\x01") { for (int i = 0; i < L_COUNT; i++) { std::wstring n = kLayoutName[i]; n.erase(std::remove(n.begin(), n.end(), L'&'), n.end()); if (Lower(n).find(arg) != std::wstring::npos) add(L"Layout: " + n, L"", [i]() { HandleCommand(110 + i); }); } return; }
    arg = starts(L"profile ");
    if (arg != L"\x01") { for (auto& n : ProfileNames()) if (Lower(n).find(arg) != std::wstring::npos) add(L"Switch to profile: " + n, L"", [n]() { SwitchProfile(n); RefreshAll(); }); return; }
    arg = starts(L"pin");
    if (arg != L"\x01" && (arg.empty() || arg[0] == L' ')) {
        std::wstring label = q.size() > 4 ? q.substr(4) : L"Pinned";
        long long u = NowMin() + g_shift; CTime lt = ClockAt(g_localClock, (ULONGLONG)u * TPM);
        add(L"Pin “" + label + L"”", L"at " + Lower(FmtHMFull(lt.st)) + L" " + kDays[lt.st.wDayOfWeek] + L" (your time)", [label, u]() {
            g_pins.push_back({ label, u }); std::sort(g_pins.begin(), g_pins.end(), [](const Pin& a, const Pin& b) { return a.utc < b.utc; }); SavePins(); RefreshAll(); });
        return;
    }
    struct Kw { const wchar_t* w; const wchar_t* title; int cmd; };
    static const Kw kws[] = {
        { L"meeting", L"Find next meeting slot", 232 }, { L"slot", L"Find next meeting slot", 232 }, { L"card", L"Meeting card / invite…", 241 },
        { L"invite", L"Meeting card / invite…", 241 }, { L"fair", L"Fairness report", 243 }, { L"dst", L"Daylight-saving radar", 244 },
        { L"daylight", L"Daylight-saving radar", 244 }, { L"settings", L"Settings…", 246 }, { L"preferences", L"Settings…", 246 },
        { L"share", L"Copy share code", 247 }, { L"export", L"Copy share code", 247 }, { L"import", L"Import share code…", 248 },
        { L"now", L"Back to now (reset time shift)", 180 }, { L"reset", L"Back to now (reset time shift)", 180 }, { L"jump", L"Jump to a time…", 231 },
        { L"alarm", L"Add alarm or countdown…", 230 }, { L"countdown", L"Add alarm or countdown…", 230 }, { L"calendar", L"Connect calendar link…", 249 },
        { L"update", L"Check for updates now", 254 }, { L"hide", L"Hide / show widget", 182 }, { L"show", L"Hide / show widget", 182 },
        { L"convert", L"Convert time from clipboard", 240 }, { L"paste", L"Convert time from clipboard", 240 }, { L"profile", L"New profile…", 256 },
    };
    for (auto& k : kws) if (wcslen(k.w) >= t.size() ? wcsncmp(k.w, t.c_str(), t.size()) == 0 && t.size() >= 2 : false) { int cmd = k.cmd; add(k.title, L"command", [cmd]() { HandleCommand(cmd); }); }
    // time calculator: a time and two or more places ("9pm in india, what time in germany")
    CalcAnswer ca;
    bool calc = TimeCalc(q, ca);
    if (calc) {
        // Enter copies and keeps the answer on screen (with a tick) so it's clear it worked
        for (auto& r : ca.rows) { std::wstring c1 = r.copy; if (g_palItems.size() < 7) add(r.title, r.sub + L"  ·  Enter copies", [c1]() { CopyText(c1); }, true); }
        if (ca.rows.size() > 1) { std::wstring copy = ca.copy; add(L"Copy all of these", L"one line per place", [copy]() { CopyText(copy); }, true); }
        WhenResult cw = ca.when;
        add(L"Show this moment on all my clocks", L"moves the widget to that time; middle-click it to come back", [cw]() { ConvertTo(cw); });
    }
    // a time expression
    WhenResult w;
    if (!calc && ParseWhen(q, w)) {
        std::wstring line = ConversionLine(w);
        add(line, L"Enter: show it on the clocks", [w]() { ConvertTo(w); });
        add(L"Copy: " + line, L"copy the conversion", [line]() { CopyText(line); });
    }
    // places: your clocks first, then known cities
    Frame fr = MakeFrame();
    for (size_t i = 0; i < g_clocks.size(); i++) {
        if (Lower(DisplayName((int)i)).find(t) == std::wstring::npos) continue;
        const CTime& ct = fr.t[i]; int st = ClockStatusAt(g_clocks[i], ct.st);
        add(DisplayName((int)i) + L"  " + Lower(FmtHMFull(ct.st)) + L" " + kDays[ct.st.wDayOfWeek],
            (g_clocks[i].kind == 1 ? std::wstring(L"your time") : OffShort(ct.offset - fr.local.offset)) + L" · " + Lower(kStatus[st]), nullptr);
    }
    if (t.size() >= 2) for (auto& a : kAliases) {
        std::wstring city = a.city; std::wstring shortCity = city.substr(0, city.find(L','));
        if (Lower(city).find(t) == std::wstring::npos) continue;
        bool have = false; for (auto& c : g_clocks) if (_wcsicmp(c.label.c_str(), shortCity.c_str()) == 0) have = true;
        if (have) continue;
        Clock z = ZoneClock(a.tz); CTime ct = ClockAt(z, fr.utc);
        std::wstring tz = a.tz;
        add(city + L"  " + Lower(FmtHMFull(ct.st)) + L" " + kDays[ct.st.wDayOfWeek], OffShort(ct.offset - fr.local.offset) + L" · Enter adds it as a clock",
            [shortCity, tz]() { PalAddClocks({ { shortCity, tz } }); });
    }
    if (g_palItems.empty()) add(L"Nothing matches “" + q + L"”", L"try: 3pm est · add tokyo · meeting · theme paper", nullptr, true);
}
static void PalRun(int i) {
    if (i < 0 || i >= (int)g_palItems.size()) return;
    PalItem it = g_palItems[i];
    if (!it.run) return;
    if (it.keepOpen) {
        it.run();
        if (i < (int)g_palItems.size() && g_palItems[i].sub.find(L"Enter copies") != std::wstring::npos) {
            std::wstring& sub = g_palItems[i].sub; sub.replace(sub.find(L"Enter copies"), 12, L"\u2713 copied to clipboard");
            g_palItems[i].copied = true; InvalidateRect(g_pal, nullptr, FALSE);
        }
        return;
    }
    PalClose(); it.run();
}
static WNDPROC g_palEditOld = nullptr;
static LRESULT CALLBACK PalEditProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_KEYDOWN) {
        if (wp == VK_DOWN || wp == VK_UP) {
            int n = (int)g_palItems.size(); if (n) { g_palSel = (g_palSel + (wp == VK_DOWN ? 1 : n - 1)) % n; InvalidateRect(g_pal, nullptr, FALSE); }
            return 0;
        }
        if (wp == VK_RETURN) { PalRun(g_palSel); return 0; }
        if (wp == VK_ESCAPE) { PalClose(); return 0; }
    }
    if (m == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;
    return CallWindowProcW(g_palEditOld, h, m, wp, lp);
}
static float PalScale() { return DpiOf(g_pal) / 96.f * g_textScale; }
static void PalLayout() {
    float s = PalScale(); int rows = std::max(1, (int)g_palItems.size());
    int W = (int)(640 * s), H = (int)((60 + rows * 40 + 10) * s);
    RECT r; GetWindowRect(g_pal, &r);
    SetWindowPos(g_pal, nullptr, r.left, r.top, W, H, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_palEdit, nullptr, (int)(46 * s), (int)(17 * s), W - (int)(64 * s), (int)(28 * s), SWP_NOZORDER | SWP_NOACTIVATE);
}
static LRESULT CALLBACK PalProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        if ((HWND)lp == g_palEdit && HIWORD(wp) == EN_CHANGE) {
            int n = GetWindowTextLengthW(g_palEdit); std::wstring q(n + 1, L'\0'); GetWindowTextW(g_palEdit, &q[0], n + 1); q.resize(n);
            PalBuild(q); PalLayout(); InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_CTLCOLOREDIT: SetTextColor((HDC)wp, RGB(0xE8, 0xEA, 0xF5)); SetBkColor((HDC)wp, RGB(0x0A, 0x0D, 0x1F)); return (LRESULT)g_palBrush;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc); int W = rc.right, H = rc.bottom;
        Surface sf; EnsureSurface(sf, W, H);
        {
            G::Bitmap bmp(W, H, W * 4, PixelFormat32bppPARGB, (BYTE*)sf.bits);
            G::Graphics g(&bmp); SetupGraphics(g); g.SetTextRenderingHint(G::TextRenderingHintClearTypeGridFit);
            float s = PalScale(); g_optScale = s;
            ThemeC th = GetTheme(g_highContrast ? T_HC : T_ELECTRIC);
            g.Clear(G::Color(th.bg | 0xFF000000));
            G::Pen bp{ G::Color(th.border), 1 }; g.DrawRectangle(&bp, 0.5f, 0.5f, W - 1.f, H - 1.f);
            DT(g, L"›", F(FMS, 22 * s), 18 * s, 39 * s, th.accent, AL);
            G::Pen sp{ G::Color(th.border), 1 }; g.DrawLine(&sp, 0.f, 58 * s, (float)W, 58 * s);
            for (size_t i = 0; i < g_palItems.size(); i++) {
                float y = (62 + 40 * i) * s;
                if ((int)i == g_palSel) { G::SolidBrush hb{ G::Color(th.hover) }; g.FillRectangle(&hb, 6 * s, y, W - 12 * s, 38 * s);
                                          G::SolidBrush ab{ G::Color(th.accent) }; g.FillRectangle(&ab, 6 * s, y + 6 * s, 3 * s, 26 * s); }
                DT(g, g_palItems[i].title, F(FS, 14 * s), 22 * s, y + 18 * s, th.fg, AL, W - 44 * s);
                DT(g, g_palItems[i].sub, F(FR, 11.5f * s), 22 * s, y + 33 * s, g_palItems[i].copied ? th.accent : th.dim, AL, W - 44 * s);
            }
        }
        BitBlt(dc, 0, 0, W, H, sf.dc, 0, 0, SRCCOPY);
        FreeSurface(sf);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        float s = PalScale(); int y = GET_Y_LPARAM(lp), i = (int)((y / s - 62) / 40);
        if (y / s >= 62 && i >= 0 && i < (int)g_palItems.size() && i != g_palSel) { g_palSel = i; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        float s = PalScale(); int y = GET_Y_LPARAM(lp), i = (int)((y / s - 62) / 40);
        if (y / s >= 62 && i >= 0 && i < (int)g_palItems.size()) PalRun(i);
        return 0;
    }
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE && g_pal == h) PostMessageW(h, WM_CLOSE, 0, 0); return 0;
    case WM_CLOSE: if (g_pal == h) PalClose(); else DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static void ShowPalette() {
    if (g_pal) { SetForegroundWindow(g_pal); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{ sizeof(wc) }; wc.hInstance = g_inst; wc.lpfnWndProc = PalProc; wc.lpszClassName = L"WinClockPalette"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc); reg = true;
    }
    POINT pt; GetCursorPos(&pt);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi);
    g_pal = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"WinClockPalette", L"WinClock", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    DWORD corner = 2; DwmSetWindowAttribute(g_pal, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
    float s = PalScale();
    if (!g_palBrush) g_palBrush = CreateSolidBrush(RGB(0x0A, 0x0D, 0x1F));
    if (g_palFont) DeleteObject(g_palFont);
    g_palFont = CreateFontW(-(int)(18 * s), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable Text");
    g_palEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, g_pal, nullptr, g_inst, nullptr);
    SendMessageW(g_palEdit, WM_SETFONT, (WPARAM)g_palFont, TRUE);
    SendMessageW(g_palEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"9pm india to germany · 3pm ny thu · add berlin · meeting");
    g_palEditOld = (WNDPROC)SetWindowLongPtrW(g_palEdit, GWLP_WNDPROC, (LONG_PTR)PalEditProc);
    PalBuild(L"");
    int W = (int)(640 * s);
    SetWindowPos(g_pal, HWND_TOPMOST, (mi.rcWork.left + mi.rcWork.right) / 2 - W / 2, mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top) / 5, W, 100, SWP_NOACTIVATE);
    PalLayout();
    ShowWindow(g_pal, SW_SHOW); SetForegroundWindow(g_pal); SetFocus(g_palEdit);
}

// ---------------------------------------------------------------- converting a time (clipboard, palette)
static void ConvertTo(const WhenResult& w) {
    g_shift = std::clamp(w.utc - NowMin(), -60LL * 1440, 60LL * 1440);
    g_conv.on = true; g_conv.line = ConversionLine(w);
    SYSTEMTIME st = w.zone.fixedOff != INT_MIN ? StOfMin(w.utc + w.zone.fixedOff) : ClockAt(w.zone.clock, (ULONGLONG)w.utc * TPM).st;
    g_conv.desc = Lower(FmtHMFull(st)) + L" " + w.zone.name + L" (" + Lower(kDays[st.wDayOfWeek]) + L")";
    ShowFlyoutAlways();
}
static void ConvertClipboard() {
    std::wstring text = ClipboardText();
    WhenResult w; std::wstring err;
    if (text.empty()) { Balloon(L"Nothing to convert", L"Copy some text with a time first, e.g. “3pm EST Thursday”, then press " + HotkeyText(S.hkConvert) + L"."); return; }
    if (!ParseWhen(text, w, &err)) {
        std::wstring snip = text.substr(0, 60); for (auto& c : snip) if (c == L'\r' || c == L'\n') c = L' ';
        Balloon(L"No time found in the clipboard", L"“" + snip + (text.size() > 60 ? L"…" : L"") + L"”\n" + err);
        return;
    }
    CalcAnswer ca;
    if (TimeCalc(text, ca) && !ca.rows.empty()) {
        std::wstring body;
        for (size_t i = 0; i < ca.rows.size() && i < 4; i++) body += (i ? L"\n" : L"") + ca.rows[i].title;
        Balloon(ca.rows[0].sub.substr(0, ca.rows[0].sub.find(L"  ·  ")), body);
    }
    ConvertTo(w);
}
