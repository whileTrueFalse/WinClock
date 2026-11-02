// ---------------------------------------------------------------- settings window (tabs)
static void FillCombo(HWND d, int id, const std::vector<std::wstring>& items, int sel) {
    SendDlgItemMessageW(d, id, CB_RESETCONTENT, 0, 0);
    for (auto& s : items) SendDlgItemMessageW(d, id, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    SendDlgItemMessageW(d, id, CB_SETCURSEL, sel, 0);
}
static int ComboSel(HWND d, int id) { return (int)SendDlgItemMessageW(d, id, CB_GETCURSEL, 0, 0); }
static void Check(HWND d, int id, bool on) { CheckDlgButton(d, id, on ? BST_CHECKED : BST_UNCHECKED); }
static bool Checked(HWND d, int id) { return IsDlgButtonChecked(d, id) == BST_CHECKED; }
static std::wstring NoAmp(const wchar_t* s) { std::wstring r = s; r.erase(std::remove(r.begin(), r.end(), L'&'), r.end()); return r; }
static void ApplyAndRefresh() { SaveSettings(); ApplyWidgetStyle(); if (S.trayIcon) AddTrayIcon(); else RemoveTrayIcon(); UpdateTrayIconImage(true); RefreshAll(); }
static void ChangedPage(HWND d) { PropSheet_Changed(GetParent(d), d); }

static INT_PTR CALLBACK SetGeneralProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_INITDIALOG: {
        std::vector<std::wstring> lay, th, sz, op, hs, he;
        for (int i = 0; i < L_COUNT; i++) lay.push_back(NoAmp(kLayoutName[i]));
        for (int i = 0; i < T_COUNT; i++) th.push_back(NoAmp(kThemeName[i]));
        int szSel = 2, opSel = 0;
        for (int i = 0; i < (int)(sizeof(kScale) / sizeof(*kScale)); i++) { sz.push_back(Fmt(L"%d%%", kScale[i])); if (kScale[i] == S.scale) szSel = i; }
        for (int i = 0; i < (int)(sizeof(kOpa) / sizeof(*kOpa)); i++) { op.push_back(Fmt(L"%d%%", kOpa[i])); if (kOpa[i] == S.opacity) opSel = i; }
        for (int h = 0; h < 24; h++) hs.push_back(Fmt(L"%02d:00", h));
        for (int h = 1; h <= 24; h++) he.push_back(Fmt(L"%02d:00", h % 24));
        FillCombo(d, IDC_SG_LAYOUT, lay, S.layout); FillCombo(d, IDC_SG_THEME, th, S.theme);
        FillCombo(d, IDC_SG_SIZE, sz, szSel); FillCombo(d, IDC_SG_OPA, op, opSel);
        FillCombo(d, IDC_SG_WS, hs, S.workStart); FillCombo(d, IDC_SG_WE, he, S.workEnd - 1);
        Check(d, IDC_SG_24H, S.h24); Check(d, IDC_SG_SECS, S.secs); Check(d, IDC_SG_DATE, S.date); Check(d, IDC_SG_CHIME, S.chime);
        Check(d, IDC_SG_STARTUP, StartupEnabled()); Check(d, IDC_SG_TOP, S.zmode == Z_TOP); Check(d, IDC_SG_LOCK, S.locked);
        return TRUE;
    }
    case WM_COMMAND: if (HIWORD(wp) == CBN_SELCHANGE || HIWORD(wp) == BN_CLICKED) ChangedPage(d); break;
    case WM_NOTIFY:
        if (((NMHDR*)lp)->code == PSN_APPLY) {
            S.layout = std::max(0, ComboSel(d, IDC_SG_LAYOUT)); S.theme = std::max(0, ComboSel(d, IDC_SG_THEME));
            S.scale = kScale[std::max(0, ComboSel(d, IDC_SG_SIZE))]; S.opacity = kOpa[std::max(0, ComboSel(d, IDC_SG_OPA))];
            S.workStart = std::max(0, ComboSel(d, IDC_SG_WS)); S.workEnd = std::max(0, ComboSel(d, IDC_SG_WE)) + 1;
            S.h24 = Checked(d, IDC_SG_24H); S.secs = Checked(d, IDC_SG_SECS); S.date = Checked(d, IDC_SG_DATE); S.chime = Checked(d, IDC_SG_CHIME);
            if (Checked(d, IDC_SG_STARTUP) != StartupEnabled()) SetStartup(Checked(d, IDC_SG_STARTUP));
            S.zmode = Checked(d, IDC_SG_TOP) ? Z_TOP : Z_DESKTOP; S.locked = Checked(d, IDC_SG_LOCK);
            ApplyAndRefresh();
            SetWindowLongPtrW(d, DWLP_MSGRESULT, PSNRET_NOERROR); return TRUE;
        }
        break;
    }
    return FALSE;
}

static void FillClockList(HWND d) {
    HWND lv = GetDlgItem(d, IDC_SC_LIST);
    int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    ListView_DeleteAllItems(lv);
    for (size_t i = 0; i < g_clocks.size(); i++) {
        const Clock& c = g_clocks[i];
        LVITEMW it{}; it.mask = LVIF_TEXT; it.iItem = (int)i; std::wstring name = DisplayName((int)i); it.pszText = (LPWSTR)name.c_str();
        ListView_InsertItem(lv, &it);
        std::wstring zone = c.kind == 1 ? L"This PC" : c.tzid, hours = Fmt(L"%02d–%02d", WorkStartOf(c), WorkEndOf(c) % 24) + (c.ws < 0 ? L" (default)" : L"");
        std::wstring cc = HolidayCC(c), hol = cc.empty() ? L"—" : CountryName(cc);
        std::wstring tb = (S.tbWhich == 0 || c.taskbar) ? L"yes" : L"";
        ListView_SetItemText(lv, (int)i, 1, (LPWSTR)zone.c_str());
        ListView_SetItemText(lv, (int)i, 2, (LPWSTR)hours.c_str());
        ListView_SetItemText(lv, (int)i, 3, (LPWSTR)hol.c_str());
        ListView_SetItemText(lv, (int)i, 4, (LPWSTR)tb.c_str());
    }
    if (sel >= 0 && sel < (int)g_clocks.size()) ListView_SetItemState(lv, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    std::wstring prof = L"Profile: " + S.profile + L"   (switch profiles from the right-click menu)";
    SetDlgItemTextW(d, IDC_SC_PROFILE, prof.c_str());
}
static INT_PTR CALLBACK SetClocksProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    HWND lv = GetDlgItem(d, IDC_SC_LIST);
    switch (m) {
    case WM_INITDIALOG: {
        ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const wchar_t* cols[] = { L"Name", L"Time zone", L"Working hours", L"Holidays", L"Taskbar" };
        int widths[] = { 130, 150, 105, 105, 50 };
        float s = DpiOf(d) / 96.f;
        for (int k = 0; k < 5; k++) { LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)cols[k]; c.cx = (int)(widths[k] * s * 0.8f); ListView_InsertColumn(lv, k, &c); }
        FillClockList(d);
        return TRUE;
    }
    case WM_COMMAND: {
        int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
        switch (LOWORD(wp)) {
        case IDC_SC_ADD: ClockDialog(-1); break;
        case IDC_SC_EDIT: if (sel >= 0) ClockDialog(sel); break;
        case IDC_SC_PERSON: if (sel >= 0) PersonDialog(sel); break;
        case IDC_SC_REMOVE: if (sel >= 0) { g_clocks.erase(g_clocks.begin() + sel); SaveClocks(); } break;
        case IDC_SC_UP: if (sel > 0) { std::swap(g_clocks[sel], g_clocks[sel - 1]); SaveClocks(); ListView_SetItemState(lv, sel - 1, LVIS_SELECTED, LVIS_SELECTED); } break;
        case IDC_SC_DOWN: if (sel >= 0 && sel + 1 < (int)g_clocks.size()) { std::swap(g_clocks[sel], g_clocks[sel + 1]); SaveClocks(); ListView_SetItemState(lv, sel + 1, LVIS_SELECTED, LVIS_SELECTED); } break;
        default: return FALSE;
        }
        FillClockList(d); RefreshAll();
        return TRUE;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (nh->idFrom == IDC_SC_LIST && nh->code == NM_DBLCLK) { int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED); if (sel >= 0) { PersonDialog(sel); FillClockList(d); RefreshAll(); } return TRUE; }
        if (nh->code == PSN_SETACTIVE) FillClockList(d);
        break;
    }
    }
    return FALSE;
}

static INT_PTR CALLBACK SetTaskbarProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_INITDIALOG:
        Check(d, IDC_ST_SHOW, S.tbShow); Check(d, IDC_ST_ALL, S.tbWhich == 0); Check(d, IDC_ST_SEL, S.tbWhich == 1);
        Check(d, IDC_ST_LEFT, S.tbAnchor == 1); Check(d, IDC_ST_RIGHT, S.tbAnchor == 0); Check(d, IDC_ST_CUSTOM, S.tbAnchor == 2);
        Check(d, IDC_ST_LABELS, S.tbLabels); Check(d, IDC_ST_ROTATE, S.tbRotate); Check(d, IDC_ST_ALLMON, S.tbAllMon);
        Check(d, IDC_ST_TRAY, S.trayIcon); Check(d, IDC_ST_TRAYTIME, S.trayTime);
        return TRUE;
    case WM_COMMAND: if (HIWORD(wp) == BN_CLICKED) ChangedPage(d); break;
    case WM_NOTIFY:
        if (((NMHDR*)lp)->code == PSN_APPLY) {
            S.tbShow = Checked(d, IDC_ST_SHOW); S.tbWhich = Checked(d, IDC_ST_SEL) ? 1 : 0;
            S.tbAnchor = Checked(d, IDC_ST_LEFT) ? 1 : Checked(d, IDC_ST_CUSTOM) ? 2 : 0;
            if (S.tbAnchor == 2 && S.tbOffset < 0) S.tbOffset = 320;
            S.tbLabels = Checked(d, IDC_ST_LABELS); S.tbRotate = Checked(d, IDC_ST_ROTATE); S.tbAllMon = Checked(d, IDC_ST_ALLMON);
            S.trayIcon = Checked(d, IDC_ST_TRAY); S.trayTime = Checked(d, IDC_ST_TRAYTIME);
            if (!S.trayIcon && !S.tbShow && !S.visible) ShowWidget(true);
            g_bar.key = -1; ApplyAndRefresh();
            SetWindowLongPtrW(d, DWLP_MSGRESULT, PSNRET_NOERROR); return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK SetMoreProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    static const int warns[] = { 0, 1, 2, 5, 10, 15 };
    switch (m) {
    case WM_INITDIALOG: {
        SetDlgItemTextW(d, IDC_SM_CALURL, S.calUrl.c_str()); Check(d, IDC_SM_CALON, S.calOn);
        std::vector<std::wstring> w; int sel = 3;
        for (int i = 0; i < 6; i++) { w.push_back(warns[i] ? Fmt(L"%d min before", warns[i]) : L"No reminder"); if (warns[i] == S.calWarn) sel = i; }
        FillCombo(d, IDC_SM_WARN, w, sel);
        Check(d, IDC_SM_UPD, S.updates); Check(d, IDC_SM_AUTOPROF, S.autoProfile);
        std::wstring st = g_calError.empty() ? (S.calOn ? Fmt(L"%d upcoming meetings loaded.", (int)g_calOcc.size()) : L"") : L"Last check: " + g_calError;
        SetDlgItemTextW(d, IDC_SM_CALSTATUS, st.c_str());
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_SM_UPDNOW) { NetStart(NET_UPDATE_MANUAL, kReleasesApi); return TRUE; }
        if (HIWORD(wp) == BN_CLICKED || HIWORD(wp) == EN_CHANGE || HIWORD(wp) == CBN_SELCHANGE) ChangedPage(d);
        break;
    case WM_NOTIFY:
        if (((NMHDR*)lp)->code == PSN_APPLY) {
            wchar_t b[2048]; GetDlgItemTextW(d, IDC_SM_CALURL, b, 2048); std::wstring url = b;
            while (!url.empty() && iswspace(url.back())) url.pop_back();
            while (!url.empty() && iswspace(url.front())) url.erase(url.begin());
            if (!url.empty() && url.compare(0, 8, L"https://") != 0 && url.compare(0, 9, L"webcal://") != 0) {
                MessageBoxW(d, L"The calendar link must start with https:// (or webcal://).", L"WinClock", MB_OK | MB_ICONWARNING);
                SetWindowLongPtrW(d, DWLP_MSGRESULT, PSNRET_INVALID); return TRUE;
            }
            bool changed = url != S.calUrl;
            S.calUrl = url; S.calOn = Checked(d, IDC_SM_CALON) && !url.empty();
            S.calWarn = warns[std::max(0, ComboSel(d, IDC_SM_WARN))];
            S.updates = Checked(d, IDC_SM_UPD); S.autoProfile = Checked(d, IDC_SM_AUTOPROF);
            if (changed || !S.calOn) { g_calEvents.clear(); g_calOcc.clear(); g_calFetched = -1; g_calError.clear(); }
            if (S.calOn) CalTick();
            ApplyAndRefresh();
            SetWindowLongPtrW(d, DWLP_MSGRESULT, PSNRET_NOERROR); return TRUE;
        }
        break;
    }
    return FALSE;
}

static void CentreOnCursorMonitor(HWND hwnd) {
    POINT pt; GetCursorPos(&pt);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT r; GetWindowRect(hwnd, &r); int w = r.right - r.left, h = r.bottom - r.top;
    SetWindowPos(hwnd, nullptr, (mi.rcWork.left + mi.rcWork.right - w) / 2, (mi.rcWork.top + mi.rcWork.bottom - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static LRESULT CALLBACK SheetSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR) {
    if (msg == WM_SHOWWINDOW && wp) { CentreOnCursorMonitor(hwnd); RemoveWindowSubclass(hwnd, SheetSubclass, id); }
    else if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, SheetSubclass, id);
    return DefSubclassProc(hwnd, msg, wp, lp);
}
static int CALLBACK SheetCallback(HWND hwnd, UINT msg, LPARAM) {
    // The sheet positions itself after this callback, so centre it just before it is first shown.
    if (msg == PSCB_INITIALIZED) SetWindowSubclass(hwnd, SheetSubclass, 0x5E7, 0);
    return 0;
}
static void SettingsWindow(int startPage = 0) {
    static bool open = false; if (open) return; open = true;
    PROPSHEETPAGEW pg[4] = {};
    int ids[] = { IDD_SET_GENERAL, IDD_SET_CLOCKS, IDD_SET_TASKBAR, IDD_SET_MORE };
    DLGPROC procs[] = { SetGeneralProc, SetClocksProc, SetTaskbarProc, SetMoreProc };
    for (int i = 0; i < 4; i++) { pg[i].dwSize = sizeof(pg[i]); pg[i].hInstance = g_inst; pg[i].pszTemplate = MAKEINTRESOURCEW(ids[i]); pg[i].pfnDlgProc = procs[i]; }
    PROPSHEETHEADERW h{}; h.dwSize = sizeof(h); h.dwFlags = PSH_PROPSHEETPAGE | PSH_USEHICON | PSH_NOCONTEXTHELP | PSH_USECALLBACK; h.pfnCallback = SheetCallback;
    h.hwndParent = g_main; h.hInstance = g_inst; h.pszCaption = L"WinClock settings"; h.nPages = 4; h.ppsp = pg; h.nStartPage = startPage;
    LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &h.hIcon);
    SetForegroundWindow(g_main);
    PropertySheetW(&h);
    open = false; TrimMemory();
}
