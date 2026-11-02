// ---------------------------------------------------------------- hover peek (mini timeline above a taskbar clock)
static HWND g_peek = nullptr; static Surface g_peekSurf; static int g_peekClock = -1;
static void HidePeek() { if (g_peek && IsWindowVisible(g_peek)) ShowWindow(g_peek, SW_HIDE); g_peekClock = -1; }
static void HatchRect(G::Graphics& g, float x, float y, float w, float h, DWORD col) {
    G::HatchBrush hb(G::HatchStyle50Percent, G::Color(col), G::Color(0)); g.FillRectangle(&hb, x, y, w, h);
}
static void DrawHourStrip(G::Graphics& g, const ThemeC& th, const Clock& c, float x, float y, float w, float h, long long utcCenter, float s) {
    int off = ClockAt(c, (ULONGLONG)utcCenter * TPM).offset;
    double localStart = (double)(utcCenter - 720 + off);
    float pxPerMin = w / 1440.f;
    g.SetClip(G::RectF(x, y, w, h));
    for (double m = floor(localStart / 60) * 60; ; m += 60) {
        float cx = x + (float)((m - localStart) * pxPerMin);
        if (cx >= x + w) break;
        float cw = 60 * pxPerMin;
        int kind = HourKind(c, (long long)m, off);
        if (kind == 0) { G::SolidBrush b{ G::Color(th.blue) }; g.FillRectangle(&b, cx + 0.5f * s, y, cw - 1 * s, h); }
        else if (kind == 1) { G::SolidBrush b{ G::Color(A(th.fg, 0x1C)) }; g.FillRectangle(&b, cx + 0.5f * s, y, cw - 1 * s, h); }
        else if (kind == 3) { G::SolidBrush b{ G::Color(A(th.accent, 0x38)) }; g.FillRectangle(&b, cx + 0.5f * s, y, cw - 1 * s, h); }
        else HatchRect(g, cx + 0.5f * s, y, cw - 1 * s, h, A(th.fg, 0x30));
        int hour = (int)(((long long)floor(m / 60) % 24 + 24) % 24);
        if (cw >= 11 * s && hour % 3 == 0) {
            int hh = S.h24 ? hour : (hour % 12 == 0 ? 12 : hour % 12);
            DT(g, std::to_wstring(hh), F(FM, 9 * s), cx + cw / 2, y + h / 2 + 3 * s, kind == 0 ? th.headFg : th.dim, AC);
        }
    }
    g.ResetClip();
    float nx = x + w / 2; G::Pen p{ G::Color(th.accent), 2 * s }; g.DrawLine(&p, nx, y - 3 * s, nx, y + h + 3 * s);
}
static void ShowPeek(Bar& b, int cell) {
    if (cell < 0 || cell >= (int)b.shown.size() || cell >= (int)b.cells.size()) return;
    int ci = b.shown[cell]; if (ci < 0 || ci >= (int)g_clocks.size()) return;
    if (!g_peek) {
        g_peek = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"WinClockPeek", L"WinClock peek",
                                 WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    }
    float s = DpiOf(b.hwnd) / 96.f; g_optScale = s;
    int theme = ResolveTheme(S.theme); if (theme == T_GLASS || theme == T_TRANSPARENT) theme = T_ELECTRIC;
    ThemeC th = GetTheme(theme);
    int W = (int)(340 * s), H = (int)(124 * s);
    EnsureSurface(g_peekSurf, W, H);
    Frame fr = MakeFrame(); const CTime& t = fr.t[ci]; const Clock& c = g_clocks[ci];
    {
        G::Bitmap bmp(W, H, W * 4, PixelFormat32bppPARGB, (BYTE*)g_peekSurf.bits);
        G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0));
        if ((th.bg >> 24) >= 0xF0) g.SetTextRenderingHint(G::TextRenderingHintClearTypeGridFit);
        G::GraphicsPath p; RoundRect(p, 0.5f, 0.5f, W - 1.f, H - 1.f, 8 * s);
        G::SolidBrush bg{ G::Color(A(th.bg, 0xFA)) }; g.FillPath(&bg, &p);
        G::Pen bp{ G::Color(th.border), 1 }; g.DrawPath(&bp, &p);
        float pad = 14 * s;
        std::wstring tm = Lower(FmtHMFull(t.st));
        FontE* ft = F(FMS, 17 * s); float tw = TW(g, tm, ft);
        DT(g, tm, ft, W - pad, 28 * s, th.fg, AR);
        DT(g, DisplayName(c), F(FS, 14.5f * s), pad, 27 * s, th.fg, AL, W - 2 * pad - tw - 10 * s);
        std::wstring why; int st = ClockStatusAt(c, t.st, &why);
        std::wstring sw = Lower(kStatus[st]); FontE* fs = F(FS, 12 * s); float w1 = TW(g, sw, fs);
        DT(g, sw, fs, pad, 47 * s, st == ST_WORK ? th.accent : st == ST_FREE ? th.blue : th.dim, AL);
        long long dd = DayNum(t.st) - DayNum(fr.local.st);
        std::wstring rest = L"  ·  " + (c.kind == 1 ? std::wstring(L"your time") : OffShort(t.offset - fr.local.offset) + L" from you") +
                            (dd > 0 ? L" · tomorrow" : dd < 0 ? L" · yesterday" : L"") + (why.empty() ? L"" : L" · " + why);
        DT(g, rest, F(FR, 12 * s), pad + w1, 47 * s, th.dim, AL, W - 2 * pad - w1);
        DrawHourStrip(g, th, c, pad, 60 * s, W - 2 * pad, 20 * s, (long long)(fr.utc / TPM), s);
        DT(g, L"−12h", F(FM, 10 * s), pad, 96 * s, th.dim, AL);
        DT(g, L"now", F(FM, 10 * s), W / 2.f, 96 * s, th.accent, AC);
        DT(g, L"+12h", F(FM, 10 * s), W - pad, 96 * s, th.dim, AR);
        std::wstring foot = L"you: " + Lower(FmtHMFull(fr.local.st));
        long long day = DaysOf(t.st.wYear, t.st.wMonth, t.st.wDay), r, set2;
        if (SunForLocalDay(c, day, r, set2) == 0)
            foot += L"  ·  ☀ " + Lower(FmtHMFull(StOfMin(r + t.offset))) + L" – " + Lower(FmtHMFull(StOfMin(set2 + t.offset)));
        std::wstring dn = DstNoteFor(c); if (!dn.empty()) foot += L"  ·  " + Lower(dn.substr(0, 1)) + dn.substr(1);
        DT(g, foot, F(FM, 11 * s), pad, 114 * s, th.dim, AL, W - 2 * pad);
    }
    RECT cr = b.cells[cell]; OffsetRect(&cr, b.lastX, b.lastY);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromRect(&cr, MONITOR_DEFAULTTONEAREST), &mi);
    RECT mon = mi.rcMonitor; int gap = (int)(8 * s), x, y;
    if (b.horiz) {
        x = (cr.left + cr.right) / 2 - W / 2;
        y = (b.lastTb.top > (mon.top + mon.bottom) / 2) ? b.lastTb.top - H - gap : b.lastTb.bottom + gap;
    } else {
        y = (cr.top + cr.bottom) / 2 - H / 2;
        x = (b.lastTb.left > (mon.left + mon.right) / 2) ? b.lastTb.left - W - gap : b.lastTb.right + gap;
    }
    x = std::clamp(x, (int)mon.left + gap, (int)mon.right - W - gap); y = std::clamp(y, (int)mon.top + gap, (int)mon.bottom - H - gap);
    POINT pos{ x, y }; Present(g_peek, g_peekSurf, 255, &pos);
    SetWindowPos(g_peek, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_peekClock = ci;
}

// ---------------------------------------------------------------- live tray icon
static HICON g_trayTimeIcon = nullptr, g_appSmallIcon = nullptr; static long long g_trayIconKey = -1;
static int TrayClock() {
    for (size_t i = 0; i < g_clocks.size(); i++) if (g_clocks[i].tray) return (int)i;
    for (size_t i = 0; i < g_clocks.size(); i++) if (g_clocks[i].kind != 1) return (int)i;
    return g_clocks.empty() ? -1 : 0;
}
static void UpdateTrayIconImage(bool force) {
    if (!g_trayAdded) return;
    int ci = S.trayTime ? TrayClock() : -1;
    long long key = ci < 0 ? -2 : NowMin() * 64 + ci + (S.h24 ? 1000000000LL : 0);
    if (!force && key == g_trayIconKey) return;
    g_trayIconKey = key;
    HICON old = g_trayTimeIcon; g_trayTimeIcon = nullptr;
    if (ci < 0) {
        if (!g_appSmallIcon) LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &g_appSmallIcon);
        g_nid.hIcon = g_appSmallIcon;
    } else {
        int sz = GetSystemMetrics(SM_CXSMICON); if (sz < 16) sz = 16;
        CTime t = ClockAt(g_clocks[ci], (ULONGLONG)NowMin() * TPM);
        int h = S.h24 ? t.st.wHour : (t.st.wHour % 12 == 0 ? 12 : t.st.wHour % 12);
        std::wstring txt = std::to_wstring(h);
        G::Bitmap bmp(sz, sz, PixelFormat32bppPARGB);
        {
            G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0));
            FillRR(g, 0.5f, 0.5f, sz - 1.f, sz - 1.f, sz * 0.22f, NAVY);
            float savedOpt = g_optScale; g_optScale = 1;
            float px = sz * (txt.size() == 1 ? 0.80f : 0.66f);
            FontE* f = F(FMS, px);
            DT(g, txt, f, sz / 2.f, sz * 0.5f + px * 0.34f - sz * 0.05f, SNOW, AC);
            g_optScale = savedOpt;
            float bw = (sz - 4) * t.st.wMinute / 60.f, bh = std::max(1.f, sz / 12.f);
            G::SolidBrush pk{ G::Color(PINK) }; g.FillRectangle(&pk, 2.f, sz - bh - 1.5f, std::max(1.f, bw), bh);
        }
        bmp.GetHICON(&g_trayTimeIcon);
        g_nid.hIcon = g_trayTimeIcon;
    }
    g_nid.uFlags = NIF_ICON; Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    if (old) DestroyIcon(old);
}

// ---------------------------------------------------------------- accessibility
static void RefreshAccessibility() {
    DWORD v = 100, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Accessibility", L"TextScaleFactor", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS) v = 100;
    g_textScale = std::clamp((int)v, 100, 225) / 100.f;
    HIGHCONTRASTW hc{ sizeof(hc) };
    g_highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
}

// ---------------------------------------------------------------- monitors
static std::vector<RECT> MonitorWorkAreas() {
    std::vector<RECT> v;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR m, HDC, LPRECT, LPARAM lp) -> BOOL {
        MONITORINFO mi{ sizeof(mi) }; if (GetMonitorInfoW(m, &mi)) {
            auto* out = (std::vector<RECT>*)lp;
            if (mi.dwFlags & MONITORINFOF_PRIMARY) out->insert(out->begin(), mi.rcWork); else out->push_back(mi.rcWork);
        }
        return TRUE;
    }, (LPARAM)&v);
    return v;
}
