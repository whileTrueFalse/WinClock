// ---------------------------------------------------------------- 2.5 menus and commands
// Tools (focus timer, stopwatch, leave-by), Show (map, markets, weather, prayer), size presets, custom theme,
// language, sharing, travel mode.
enum {
    ID_FOCUS_START = 3000, ID_FOCUS_BREAK, ID_FOCUS_PAUSE, ID_FOCUS_SKIP, ID_FOCUS_STOP,
    ID_FOCUS_LEN = 3010,                    // + index into kFocusLens
    ID_SW_TOGGLE = 3020, ID_SW_LAP, ID_SW_RESET, ID_SW_COPY, ID_LEAVEBY,
    ID_MAP = 3030, ID_WEATHER, ID_WEATHER_F, ID_WEATHER_NOW, ID_PRAYER, ID_ASR_HANAFI, ID_CARDS_CLEAR,
    ID_PRAYER_METHOD = 3040,                // + method
    ID_SHARE_MSG = 3050, ID_SHARE_STATUS,
    ID_THEME_BG = 3060, ID_THEME_FG, ID_THEME_ACCENT, ID_THEME_2ND, ID_THEME_WINACCENT, ID_THEME_RESET,
    ID_PRESET = 3070,                       // + 0..3
    ID_LANG = 3080,                         // + language
    ID_MARKET = 3100,                       // + market
};
static const int kFocusLens[] = { 15, 25, 45, 50, 90 };
static const wchar_t* kLangName[] = { L"English", L"हिन्दी (Hindi)", L"Deutsch (German)", L"Español (Spanish)" };
static const int kLangCount = 4;

static HMENU BuildToolsMenu() {
    HMENU t = CreatePopupMenu();
    HMENU f = CreatePopupMenu();
    if (!g_focus.phase) {
        AppendMenuW(f, MF_STRING, ID_FOCUS_START, Fmt(L"Start focus (%d min)", S.focusMin).c_str());
        AppendMenuW(f, MF_STRING, ID_FOCUS_BREAK, Fmt(L"Start a break (%d min)", S.breakMin).c_str());
    } else {
        AppendMenuW(f, MF_STRING, ID_FOCUS_PAUSE, g_focus.paused ? L"Resume" : L"Pause");
        AppendMenuW(f, MF_STRING, ID_FOCUS_SKIP, g_focus.phase == 1 ? L"Skip to the break" : L"Skip to focus");
        AppendMenuW(f, MF_STRING, ID_FOCUS_STOP, L"Stop");
    }
    AppendMenuW(f, MF_SEPARATOR, 0, nullptr);
    HMENU len = CreatePopupMenu();
    for (int i = 0; i < 5; i++) {
        MENUITEMINFOW mi{ sizeof(mi) }; mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE; mi.fType = MFT_RADIOCHECK;
        std::wstring s = Fmt(L"%d min", kFocusLens[i]); mi.dwTypeData = (LPWSTR)s.c_str(); mi.wID = ID_FOCUS_LEN + i; mi.fState = S.focusMin == kFocusLens[i] ? MFS_CHECKED : 0;
        InsertMenuItemW(len, i, TRUE, &mi);
    }
    AppendMenuW(f, MF_POPUP, (UINT_PTR)len, L"Focus length");
    AppendMenuW(t, MF_POPUP, (UINT_PTR)f, g_focus.phase ? (L"Focus timer (" + std::wstring(FocusPhaseName(g_focus.phase)) + L" " + MsClock(FocusLeft()) + L")").c_str() : L"Focus timer");
    HMENU sw = CreatePopupMenu();
    AppendMenuW(sw, MF_STRING, ID_SW_TOGGLE, g_sw.running ? L"Pause" : (g_sw.accMs ? L"Resume" : L"Start"));
    AppendMenuW(sw, MF_STRING | (g_sw.running ? 0 : MF_GRAYED), ID_SW_LAP, L"Lap");
    AppendMenuW(sw, MF_STRING | (g_sw.laps.empty() && !g_sw.accMs ? MF_GRAYED : 0), ID_SW_COPY, L"Copy laps");
    AppendMenuW(sw, MF_STRING | (TimerActive() && !g_focus.phase ? 0 : MF_GRAYED), ID_SW_RESET, L"Reset");
    AppendMenuW(t, MF_POPUP, (UINT_PTR)sw, (g_sw.running || g_sw.accMs) ? (L"Stopwatch (" + MsClock(SwElapsed()) + L")").c_str() : L"Stopwatch");
    AppendMenuW(t, MF_STRING, ID_LEAVEBY, L"Leave-by reminder…");
    return t;
}
static HMENU BuildShowMenu() {
    HMENU m = CreatePopupMenu();
    auto chk = [](HMENU h, UINT id, const std::wstring& t, bool on, bool radio = false, bool gray = false) {
        MENUITEMINFOW mi{ sizeof(mi) }; mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
        mi.fType = radio ? MFT_RADIOCHECK : MFT_STRING; mi.fState = (on ? MFS_CHECKED : 0) | (gray ? MFS_GRAYED : 0); mi.wID = id; mi.dwTypeData = (LPWSTR)t.c_str();
        InsertMenuItemW(h, GetMenuItemCount(h), TRUE, &mi);
    };
    chk(m, ID_MAP, L"Day/night world map", S.showMap);
    HMENU mk = CreatePopupMenu();
    for (int i = 0; i < kMarketCount; i++) chk(mk, ID_MARKET + i, kMarkets[i].name, MarketOn(i));
    AppendMenuW(mk, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mk, MF_STRING | MF_GRAYED, 0, L"Regular hours; national holidays only, no half days");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)mk, MarketsShown() ? Fmt(L"Market hours (%d)", MarketsShown()).c_str() : L"Market hours");
    HMENU wx = CreatePopupMenu();
    chk(wx, ID_WEATHER, L"Show weather next to each city", S.weather);
    chk(wx, ID_WEATHER_F, L"Use °F", S.weatherF, false, !S.weather);
    AppendMenuW(wx, MF_STRING | (S.weather ? 0 : MF_GRAYED), ID_WEATHER_NOW, L"Refresh now");
    AppendMenuW(wx, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(wx, MF_STRING | MF_GRAYED, 0, L"From Open-Meteo (free); sends only city coordinates");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)wx, L"Weather");
    HMENU pr = CreatePopupMenu();
    chk(pr, ID_PRAYER, L"Show prayer times in tooltips", S.prayer);
    AppendMenuW(pr, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < kPrayerMethodCount; i++) chk(pr, ID_PRAYER_METHOD + i, kPrayerMethods[i].name, S.prayerMethod == i, true, !S.prayer);
    AppendMenuW(pr, MF_SEPARATOR, 0, nullptr);
    chk(pr, ID_ASR_HANAFI, L"Asr: Hanafi (later)", S.asrHanafi, false, !S.prayer);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)pr, L"Prayer times");
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"Moon phase is always in the tooltips");
    if (!g_cards.empty()) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, ID_CARDS_CLEAR, L"Clear answer cards"); }
    return m;
}
static void AddLookMenus(HMENU look) {   // size presets, custom colours, language (inside Appearance)
    HMENU pre = CreatePopupMenu();
    AppendMenuW(pre, MF_STRING, ID_PRESET + 0, L"Mini: one-line strip");
    AppendMenuW(pre, MF_STRING, ID_PRESET + 1, L"Small: compact list");
    AppendMenuW(pre, MF_STRING, ID_PRESET + 2, L"Medium: table");
    AppendMenuW(pre, MF_STRING, ID_PRESET + 3, L"Large: big table");
    InsertMenuW(look, 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)pre, L"Size preset");
    HMENU col = CreatePopupMenu();
    AppendMenuW(col, MF_STRING, ID_THEME_BG, L"Background colour…");
    AppendMenuW(col, MF_STRING, ID_THEME_FG, L"Text colour…");
    AppendMenuW(col, MF_STRING, ID_THEME_ACCENT, L"Accent colour…");
    AppendMenuW(col, MF_STRING, ID_THEME_2ND, L"Second colour…");
    AppendMenuW(col, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(col, MF_STRING, ID_THEME_WINACCENT, L"Use my Windows accent colour");
    AppendMenuW(col, MF_STRING, ID_THEME_RESET, L"Reset custom colours");
    AppendMenuW(look, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(look, MF_POPUP, (UINT_PTR)col, L"Custom theme colours");
    HMENU lg = CreatePopupMenu();
    for (int i = 0; i < kLangCount; i++) {
        MENUITEMINFOW mi{ sizeof(mi) }; mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE; mi.fType = MFT_RADIOCHECK;
        mi.dwTypeData = (LPWSTR)kLangName[i]; mi.wID = ID_LANG + i; mi.fState = S.lang == i ? MFS_CHECKED : 0;
        InsertMenuItemW(lg, i, TRUE, &mi);
    }
    AppendMenuW(look, MF_POPUP, (UINT_PTR)lg, L"Language");
}
static void AddShareMenus(HMENU share) {
    InsertMenuW(share, 0, MF_BYPOSITION | MF_STRING, ID_SHARE_MSG, g_shift ? L"Copy this planned time as a message" : L"Copy the current time as a message");
    InsertMenuW(share, 1, MF_BYPOSITION | MF_STRING, ID_SHARE_STATUS, L"Copy my working hours (Slack / Teams status)");
    InsertMenuW(share, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
}

// ---- custom colours
static bool PickColour(DWORD& c) {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc{ sizeof(cc) }; cc.hwndOwner = g_main; cc.lpCustColors = custom;
    cc.rgbResult = RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    SetForegroundWindow(g_main);
    if (!ChooseColorW(&cc)) return false;
    c = 0xFF000000 | (GetRValue(cc.rgbResult) << 16) | (GetGValue(cc.rgbResult) << 8) | GetBValue(cc.rgbResult);
    return true;
}
static DWORD WindowsAccent() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return 0xFF000000 | ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);   // stored as ABGR
    DWORD c = 0; BOOL op; if (SUCCEEDED(DwmGetColorizationColor(&c, &op))) return 0xFF000000 | (c & 0xFFFFFF);
    return 0xFF0078D4;
}

// ---- languages
static void ApplyLanguageFonts();
static void SetLanguage(int lang);

// ---- travel mode: the PC's time zone changed
static std::wstring g_travelOld;
static void TravelAddHome() {
    if (g_travelOld.empty()) return;
    Clock c; c.label = L"Home"; c.tzid = g_travelOld; ResolveClock(c);
    if (!c.valid) return;
    g_clocks.insert(g_clocks.begin(), c); SaveClocks(); ComputeDst(true); RefreshAll();
    Balloon(L"Home clock added", L"“Home” shows " + g_travelOld + L". Your other clocks now count from where you are.");
}
static void CheckTravel() {
    if (g_localKey.empty()) return;
    if (S.lastLocalTz.empty()) { S.lastLocalTz = g_localKey; SaveSettings(); return; }
    if (_wcsicmp(S.lastLocalTz.c_str(), g_localKey.c_str()) == 0) return;
    std::wstring old = S.lastLocalTz; S.lastLocalTz = g_localKey; SaveSettings();
    bool have = false; for (auto& c : g_clocks) if (c.kind == 0 && _wcsicmp(c.tzid.c_str(), old.c_str()) == 0) have = true;
    g_travelOld = old;
    if (!have) {
        Balloon(L"New time zone: " + g_localKey, L"Travelling? Click here to keep a “Home” clock on " + old + L".");
        g_balloonTravel = true;
    } else Balloon(L"New time zone: " + g_localKey, L"Your clocks now count from here. Your clock for " + old + L" stays as it is.");
}

static bool HandleExtraCommand(int id) {
    if (id < 3000 || id >= 3200) return false;
    switch (id) {
    case ID_FOCUS_START: FocusStart(1); break;
    case ID_FOCUS_BREAK: FocusStart(2); break;
    case ID_FOCUS_PAUSE: FocusPause(); break;
    case ID_FOCUS_SKIP: FocusStart(g_focus.phase == 1 ? 2 : 1); break;
    case ID_FOCUS_STOP: FocusStop(); break;
    case ID_SW_TOGGLE: SwToggle(); break;
    case ID_SW_LAP: SwLap(); break;
    case ID_SW_RESET: SwReset(); break;
    case ID_SW_COPY: CopyText(SwLapsText()); return true;
    case ID_LEAVEBY: {
        AlarmDialog(-1, 120); return true;
    }
    case ID_MAP: S.showMap = !S.showMap; SaveSettings(); break;
    case ID_WEATHER: S.weather = !S.weather; SaveSettings(); if (S.weather) WeatherTick(true); break;
    case ID_WEATHER_F: S.weatherF = !S.weatherF; SaveSettings(); break;
    case ID_WEATHER_NOW: WeatherTick(true); break;
    case ID_PRAYER: S.prayer = !S.prayer; SaveSettings(); break;
    case ID_ASR_HANAFI: S.asrHanafi = !S.asrHanafi; SaveSettings(); break;
    case ID_CARDS_CLEAR: g_cards.clear(); SaveCards(); break;
    case ID_SHARE_MSG: {
        WhenResult w; w.utc = NowMin() + g_shift; w.zone.clock = g_localClock; w.zone.name = L"your time"; w.hasTime = true;
        CopyText(TimeMessage(w, {})); Balloon(L"Copied", L"The time in every place is on your clipboard. Paste it into WhatsApp, Slack or email."); return true;
    }
    case ID_SHARE_STATUS: { std::wstring s = WorkHoursStatus(); CopyText(s); Balloon(L"Copied for your status", s); return true; }
    case ID_THEME_BG: if (PickColour(S.cBg)) S.theme = T_CUSTOM; break;
    case ID_THEME_FG: if (PickColour(S.cFg)) S.theme = T_CUSTOM; break;
    case ID_THEME_ACCENT: if (PickColour(S.cAccent)) S.theme = T_CUSTOM; break;
    case ID_THEME_2ND: if (PickColour(S.cBlue)) S.theme = T_CUSTOM; break;
    case ID_THEME_WINACCENT: S.cAccent = WindowsAccent(); S.cBlue = MixC(S.cAccent, 0xFFFFFFFF, 0.35f); S.theme = T_CUSTOM; break;
    case ID_THEME_RESET: { Settings d; S.cBg = d.cBg; S.cFg = d.cFg; S.cAccent = d.cAccent; S.cBlue = d.cBlue; break; }
    default:
        if (id >= ID_FOCUS_LEN && id < ID_FOCUS_LEN + 5) { S.focusMin = kFocusLens[id - ID_FOCUS_LEN]; break; }
        if (id >= ID_PRAYER_METHOD && id < ID_PRAYER_METHOD + kPrayerMethodCount) { S.prayerMethod = id - ID_PRAYER_METHOD; break; }
        if (id >= ID_MARKET && id < ID_MARKET + kMarketCount) { MarketToggle(id - ID_MARKET); break; }
        if (id >= ID_LANG && id < ID_LANG + kLangCount) { SetLanguage(id - ID_LANG); break; }
        if (id >= ID_PRESET && id < ID_PRESET + 4) {
            static const int lay[] = { L_STRIP, L_COMPACT, L_LIST, L_LIST }, sc[] = { 100, 90, 100, 130 };
            S.layout = lay[id - ID_PRESET]; S.scale = sc[id - ID_PRESET]; break;
        }
        return false;
    }
    SaveSettings(); ApplyWidgetStyle(); RefreshAll();
    return true;
}
