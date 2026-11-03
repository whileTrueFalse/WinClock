// WinClock — a tiny native world-clock widget for the Windows desktop and taskbar.
// Pure Win32 + GDI+, single executable, no runtime dependencies.
// Design: terminal / TUI — deep navy, periwinkle, hot pink, monospace.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <gdiplus.h>
#include <psapi.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <climits>
#include <map>
#include <functional>
#include <commdlg.h>
#include <winhttp.h>
#include <wincrypt.h>
#include "resource.h"
#include "version.h"
#if defined(TEST_BUILD) || defined(WINE_SHIM)   // Wine lacks the *Ex variants; the classic ones use the same leading fields
#define SystemTimeToTzSpecificLocalTimeEx(d, u, l) SystemTimeToTzSpecificLocalTime((const TIME_ZONE_INFORMATION*)(d), u, l)
#define TzSpecificLocalTimeToSystemTimeEx(d, l, u) TzSpecificLocalTimeToSystemTime((const TIME_ZONE_INFORMATION*)(d), l, u)
#endif

namespace G = Gdiplus;

// ---------------------------------------------------------------- palette
// Terminal design: deep navy, periwinkle, hot pink, soft white
static const DWORD SNOW        = 0xFFE8EAF5;   // text white
static const DWORD NAVY        = 0xFF0A0D1F;   // background
static const DWORD NAVY_DEEP   = 0xFF060818;
static const DWORD ELECTRIC    = 0xFF5B7BFF;   // periwinkle
static const DWORD ELECTRIC_HI = 0xFF7D96FF;
static const DWORD ICE         = 0xFF9AA3D6;   // dim text
static const DWORD PINK        = 0xFFFF2D87;   // hot pink
static const DWORD PINK_SOFT   = 0xFFFF7AB6;

static inline DWORD A(DWORD c, int a) { return ((DWORD)(a & 0xFF) << 24) | (c & 0x00FFFFFF); }

// ---------------------------------------------------------------- types
enum Layout { L_LIST, L_ANALOG, L_TIMELINE, L_COMPACT, L_STRIP, L_COUNT };
static const wchar_t* kLayoutKey[]  = { L"list", L"analog", L"timeline", L"compact", L"strip" };
static const wchar_t* kLayoutName[] = { L"&Table", L"&Analog faces", L"&Timeline planner", L"&Compact", L"&Mini strip (one line)" };

enum Theme { T_ELECTRIC, T_SNOW, T_PINK, T_BLUE, T_GLASS, T_TRANSPARENT, T_AUTO, T_CUSTOM, T_COUNT, T_HC };
static const wchar_t* kThemeKey[]  = { L"terminal", L"paper", L"synthwave", L"periwinkle", L"glass", L"transparent", L"auto", L"custom" };
static const wchar_t* kThemeName[] = { L"&Terminal (dark)", L"&Paper (light)", L"&Synthwave", L"&Periwinkle", L"G&lass", L"T&ransparent", L"&Auto (follow Windows light/dark)", L"&Custom (your colours)" };

enum ZMode { Z_DESKTOP, Z_TOP };

struct ThemeC {
    DWORD bg, outline, border, fg, dim, sep, hover, accent, blue, headFg, face, statusBg, statusFg, mode;
    bool shadow;
};

static int g_appsLight = -1;
static bool g_highContrast = false;   // Windows high-contrast mode overrides every theme
static float g_textScale = 1.0f;      // Windows "Text size" accessibility setting
static int ResolveTheme(int t) {
    if (g_highContrast) return T_HC;
    if (t != T_AUTO) return t;
    if (g_appsLight < 0) {
        DWORD v = 0, sz = sizeof(v);
        g_appsLight = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                   L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS && v != 0;
    }
    return g_appsLight ? T_SNOW : T_ELECTRIC;
}

static ThemeC CustomTheme();   // built from the user's colours (defined after the settings)
static DWORD SysCol(int idx) { COLORREF c = GetSysColor(idx); return 0xFF000000 | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c); }
static ThemeC GetTheme(int t) {
    switch (t) {
    case T_HC:
        return { SysCol(COLOR_WINDOW), SysCol(COLOR_WINDOWTEXT), SysCol(COLOR_WINDOWTEXT), SysCol(COLOR_WINDOWTEXT), SysCol(COLOR_WINDOWTEXT),
                 SysCol(COLOR_GRAYTEXT), SysCol(COLOR_HIGHLIGHT), SysCol(COLOR_HOTLIGHT), SysCol(COLOR_HIGHLIGHT), SysCol(COLOR_HIGHLIGHTTEXT),
                 SysCol(COLOR_WINDOW), SysCol(COLOR_HIGHLIGHT), SysCol(COLOR_HIGHLIGHTTEXT), SysCol(COLOR_HIGHLIGHTTEXT), false };
    case T_SNOW:  // paper
        return { A(0xFFF5F6FF, 0xF7), A(ELECTRIC, 0x40), 0xFFC3C9EE, NAVY, 0xFF4A537F, 0xFFDDE1F7, 0xFFE2E6FF,
                 0xFFE5137A, 0xFF4B67F5, 0xFFFFFFFF, 0xFFFFFFFF, NAVY, SNOW, PINK, false };
    case T_PINK:  // synthwave
        return { A(0xFF150A26, 0xF5), A(PINK, 0x45), 0xFF4A2468, 0xFFFCEAF5, 0xFFB79AD0, 0xFF2A1440, 0xFF2A1446,
                 PINK, ELECTRIC_HI, 0xFF150A26, 0xFF1E0F35, PINK, 0xFFFFFFFF, 0xFF150A26, false };
    case T_BLUE:  // periwinkle
        return { A(ELECTRIC, 0xF5), A(0xFFFFFFFF, 0x50), 0xFF93A7FF, 0xFFFFFFFF, 0xFFDDE3FF, 0xFF7690FF, 0xFF6D89FF,
                 NAVY, SNOW, NAVY, 0xFF4A68EE, NAVY, SNOW, PINK, false };
    case T_GLASS:
        return { A(NAVY, 0x99), A(ELECTRIC, 0x50), A(ELECTRIC, 0x70), SNOW, 0xFFC9D0F5, A(SNOW, 0x22), A(ELECTRIC, 0x40),
                 PINK, ELECTRIC, NAVY, A(NAVY, 0xC8), A(0xFFE6E8FF, 0xE6), NAVY, PINK, true };
    case T_TRANSPARENT:
        return { 0x01000000, 0, A(SNOW, 0x45), SNOW, 0xFFD0D6F5, A(SNOW, 0x22), A(ELECTRIC, 0x40),
                 PINK, ELECTRIC, NAVY, A(NAVY, 0x9A), A(NAVY, 0xB0), SNOW, PINK, true };
    case T_CUSTOM: return CustomTheme();
    default:      // terminal
        return { A(NAVY, 0xF6), A(ELECTRIC, 0x30), 0xFF2B3266, SNOW, 0xFFA9B1DC, 0xFF1E2550, 0xFF1A2152,
                 PINK, ELECTRIC, NAVY, 0xFF11163A, 0xFFE6E8FF, NAVY, PINK, false };
    }
}

struct Clock {
    std::wstring label, tzid;
    bool taskbar = false;
    int kind = 0;                 // 0 = Windows zone, 1 = system local, 2 = UTC
    DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
    bool valid = false;
    // people & places
    std::wstring person;          // "Alex" -> shown as "Alex · NYC"
    std::wstring note;            // out-of-office or any short note
    std::wstring cc;              // holiday country: "" = automatic, "-" = none
    int ws = -1, we = -1;         // working hours (-1 = use the global setting)
    int offMask = 0x41;           // days off, bit 0 = Sunday .. bit 6 = Saturday
    int awayY = 0, awayM = 0, awayD = 0;   // away until this date (inclusive)
    double lat = NAN, lon = NAN;  // for sunrise / sunset
    bool tray = false;            // this clock's hour is drawn in the tray icon
};

struct CTime { SYSTEMTIME st; int offset; bool dst; };

struct Settings {
    int layout = L_LIST, theme = T_ELECTRIC, opacity = 100, scale = 100;
    bool h24 = false, secs = true, date = true;
    int zmode = Z_DESKTOP; bool locked = false, clickThrough = false;
    int x = INT_MIN, y = INT_MIN; bool visible = true;
    bool tbShow = true, tbRotate = false, tbLabels = true, trayIcon = true; int tbOffset = -1, tbAnchor = 0;
    int workStart = 9, workEnd = 18, cols = 3, flyLayout = L_ANALOG;
    bool hotkey = true, chime = false;
    WORD hkShow = 0x0657, hkConvert = 0x0656, hkPalette = 0x064B;   // HOTKEYF mods << 8 | VK: Ctrl+Alt+W / V / K
    int tbWhich = 0;              // 0 = every clock on the taskbar, 1 = only the ticked ones
    bool tbAllMon = false, trayTime = false, updates = false, autoProfile = false;
    std::wstring profile = L"Default";
    std::wstring calUrl; bool calOn = false; int calWarn = 5;
    int lastUpdDay = 0;
    // 2.5 extras
    bool showMap = false, weather = false, weatherF = false, prayer = false, asrHanafi = false;
    int prayerMethod = 0, focusMin = 25, breakMin = 5, longMin = 15, lang = 0;
    std::wstring markets, lastLocalTz;
    DWORD cBg = 0xFF101418, cFg = 0xFFEDEFF5, cAccent = 0xFF34D399, cBlue = 0xFF60A5FA;   // custom theme
};

struct Surface { HDC dc = nullptr; HBITMAP bmp = nullptr, old = nullptr; void* bits = nullptr; int w = 0, h = 0; };

struct Hit { RECT rc; int clock; };

struct Tip {
    HWND tt = nullptr, owner = nullptr; std::wstring text; bool shown = false;
};

struct View {
    HWND hwnd = nullptr; bool fly = false;
    Surface surf; int W = 0, H = 0;
    std::vector<Hit> hits; int hover = -1;
    RECT tlBar{}; bool tlHover = false; int tlMin = 0;   // timeline hover offset (minutes from now)
    long long key = -1; bool tracking = false;
    Tip tip; RECT flashRc{};
    BYTE animAlpha = 255; DWORD animStart = 0; int animX = 0, animY = 0, animDX = 0, animDY = 0;
    bool dragging = false, moved = false; POINT dragPt{}; RECT dragRc{};   // our own drag (free movement in any direction)
};

struct Bar {
    HWND hwnd = nullptr; Surface surf; int W = 0, H = 0;
    std::vector<int> items; std::vector<RECT> cells; int hover = -1;
    bool horiz = true, hiddenFs = false, visible = false;
    int rot = 0, rotCount = 0; long long key = -1;
    RECT lastTb{}, lastTray{}; int lastX = INT_MIN, lastY = INT_MIN;
    bool down = false, dragging = false; POINT downPt{}; int downX = 0, downY = 0;
    bool tracking = false; Tip tip; bool light = false; RECT timerRc{};
    bool autoRotate = false; std::vector<int> shown;
};

// ---------------------------------------------------------------- globals
static HINSTANCE g_inst;
static HWND g_main;
static std::wstring g_ini;
static Settings S;
static const wchar_t* T(const wchar_t* en);   // i18n.inc
static std::vector<Clock> g_clocks;
static Clock g_localClock;
static long long g_shift = 0;         // minutes the whole display is shifted (planning)
static bool g_paused = false;
static int g_localBias = 0; static std::wstring g_localKey;
static View g_widget, g_flyout;
static Bar g_bar;
static bool g_deskRaised = false;
static DWORD g_flyClosedAt = 0;
static UINT WM_TASKBARCREATED = 0;
static HWINEVENTHOOK g_hook = nullptr;
static NOTIFYICONDATAW g_nid{};
static HPOWERNOTIFY g_powerNotify = nullptr;
static ULONG_PTR g_gdipToken = 0;

static const ULONGLONG TPM = 600000000ULL;  // 100ns ticks per minute
#define WM_APP_TRAY  (WM_APP + 1)
#define WM_APP_SHOW  (WM_APP + 2)
enum { TM_TICK = 1, TM_BAR = 2, TM_TIP = 3, TM_ALARM = 4, TM_PEEK = 8 };
#define WM_APP_NET   (WM_APP + 3)
#define APP_VERSION_FALLBACK L"2.2.0"

static const GUID kDisplayState = { 0x6fe69556, 0x704a, 0x47a0, { 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47 } };

static const wchar_t* kDays[]   = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
static const wchar_t* kDaysL[]  = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
static const wchar_t* kMonths[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
static const wchar_t* kMonthsL[]= { L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December" };

static DWORD MixC(DWORD a, DWORD b, float t) {   // blend two colours, t = share of b
    auto ch = [&](int sh) { return (DWORD)std::clamp((int)lroundf(((a >> sh) & 0xFF) * (1 - t) + ((b >> sh) & 0xFF) * t), 0, 255) << sh; };
    return 0xFF000000 | ch(16) | ch(8) | ch(0);
}
static bool IsLightC(DWORD c) { return (((c >> 16) & 0xFF) * 299 + ((c >> 8) & 0xFF) * 587 + (c & 0xFF) * 114) / 1000 > 140; }
static ThemeC CustomTheme() {
    DWORD bg = S.cBg | 0xFF000000, fg = S.cFg | 0xFF000000, ac = S.cAccent | 0xFF000000, b2 = S.cBlue | 0xFF000000;
    bool light = IsLightC(bg);
    return { A(bg, 0xF6), A(b2, 0x40), MixC(bg, b2, 0.35f), fg, MixC(fg, bg, 0.35f), MixC(bg, fg, 0.12f), MixC(bg, b2, 0.18f),
             ac, b2, IsLightC(b2) ? 0xFF101010 : 0xFFFFFFFF, MixC(bg, fg, 0.06f), light ? 0xFF1A1A1A : MixC(fg, bg, 0.05f),
             light ? 0xFFF2F2F2 : MixC(bg, bg, 0), ac, false };
}

// ---------------------------------------------------------------- city aliases
struct Alias { const wchar_t* city; const wchar_t* tz; const wchar_t* cc; double lat, lon; };
static const Alias kAliases[] = {
    { L"Abu Dhabi, UAE", L"Arabian Standard Time", L"AE", 24.45, 54.38 },
    { L"Adelaide, Australia", L"Cen. Australia Standard Time", L"AU", -34.93, 138.60 },
    { L"Ahmedabad, India", L"India Standard Time", L"IN", 23.02, 72.57 },
    { L"Amsterdam, Netherlands", L"W. Europe Standard Time", L"NL", 52.37, 4.90 },
    { L"Anchorage, Alaska", L"Alaskan Standard Time", L"US", 61.22, -149.90 },
    { L"Athens, Greece", L"GTB Standard Time", L"", 37.98, 23.73 },
    { L"Atlanta, USA", L"Eastern Standard Time", L"US", 33.75, -84.39 },
    { L"Auckland, New Zealand", L"New Zealand Standard Time", L"NZ", -36.85, 174.76 },
    { L"Austin, USA", L"Central Standard Time", L"US", 30.27, -97.74 },
    { L"Azores, Portugal", L"Azores Standard Time", L"", 37.74, -25.67 },
    { L"Baghdad, Iraq", L"Arabic Standard Time", L"", 33.31, 44.36 },
    { L"Bangkok, Thailand", L"SE Asia Standard Time", L"", 13.76, 100.50 },
    { L"Barcelona, Spain", L"Romance Standard Time", L"ES", 41.39, 2.17 },
    { L"Beijing, China", L"China Standard Time", L"CN", 39.90, 116.41 },
    { L"Bengaluru, India", L"India Standard Time", L"IN", 12.97, 77.59 },
    { L"Berlin, Germany", L"W. Europe Standard Time", L"DE", 52.52, 13.40 },
    { L"Bogotá, Colombia", L"SA Pacific Standard Time", L"", 4.71, -74.07 },
    { L"Boston, USA", L"Eastern Standard Time", L"US", 42.36, -71.06 },
    { L"Brisbane, Australia", L"E. Australia Standard Time", L"AU", -27.47, 153.03 },
    { L"Brussels, Belgium", L"Romance Standard Time", L"", 50.85, 4.35 },
    { L"Bucharest, Romania", L"GTB Standard Time", L"", 44.43, 26.10 },
    { L"Budapest, Hungary", L"Central Europe Standard Time", L"", 47.50, 19.04 },
    { L"Buenos Aires, Argentina", L"Argentina Standard Time", L"", -34.60, -58.38 },
    { L"Cairo, Egypt", L"Egypt Standard Time", L"", 30.04, 31.24 },
    { L"Calgary, Canada", L"Mountain Standard Time", L"CA", 51.05, -114.07 },
    { L"Canary Islands, Spain", L"GMT Standard Time", L"ES", 28.12, -15.43 },
    { L"Cancún, Mexico", L"Eastern Standard Time (Mexico)", L"MX", 21.16, -86.85 },
    { L"Cape Town, South Africa", L"South Africa Standard Time", L"", -33.92, 18.42 },
    { L"Caracas, Venezuela", L"Venezuela Standard Time", L"", 10.48, -66.90 },
    { L"Casablanca, Morocco", L"Morocco Standard Time", L"", 33.57, -7.59 },
    { L"Chatham Islands, New Zealand", L"Chatham Islands Standard Time", L"NZ", -43.95, -176.56 },
    { L"Chennai, India", L"India Standard Time", L"IN", 13.08, 80.27 },
    { L"Chicago, USA", L"Central Standard Time", L"US", 41.88, -87.63 },
    { L"Chihuahua, Mexico", L"Mountain Standard Time (Mexico)", L"MX", 28.63, -106.07 },
    { L"Colombo, Sri Lanka", L"Sri Lanka Standard Time", L"", 6.93, 79.85 },
    { L"Copenhagen, Denmark", L"Romance Standard Time", L"", 55.68, 12.57 },
    { L"Cuiabá, Brazil", L"Central Brazilian Standard Time", L"BR", -15.60, -56.10 },
    { L"Dallas, USA", L"Central Standard Time", L"US", 32.78, -96.80 },
    { L"Darwin, Australia", L"AUS Central Standard Time", L"AU", -12.46, 130.84 },
    { L"Delhi, India", L"India Standard Time", L"IN", 28.61, 77.21 },
    { L"Denver, USA", L"Mountain Standard Time", L"US", 39.74, -104.99 },
    { L"Dhaka, Bangladesh", L"Bangladesh Standard Time", L"", 23.81, 90.41 },
    { L"Doha, Qatar", L"Arab Standard Time", L"", 25.29, 51.53 },
    { L"Dubai, UAE", L"Arabian Standard Time", L"AE", 25.20, 55.27 },
    { L"Dublin, Ireland", L"GMT Standard Time", L"IE", 53.35, -6.26 },
    { L"Easter Island, Chile", L"Easter Island Standard Time", L"", -27.11, -109.35 },
    { L"Edmonton, Canada", L"Mountain Standard Time", L"CA", 53.55, -113.49 },
    { L"Fortaleza, Brazil", L"SA Eastern Standard Time", L"BR", -3.73, -38.53 },
    { L"Frankfurt, Germany", L"W. Europe Standard Time", L"DE", 50.11, 8.68 },
    { L"Geneva, Switzerland", L"W. Europe Standard Time", L"", 46.20, 6.14 },
    { L"Halifax, Canada", L"Atlantic Standard Time", L"CA", 44.65, -63.57 },
    { L"Helsinki, Finland", L"FLE Standard Time", L"", 60.17, 24.94 },
    { L"Hobart, Australia", L"Tasmania Standard Time", L"AU", -42.88, 147.33 },
    { L"Ho Chi Minh City, Vietnam", L"SE Asia Standard Time", L"", 10.82, 106.63 },
    { L"Hong Kong", L"China Standard Time", L"", 22.32, 114.17 },
    { L"Honolulu, Hawaii", L"Hawaiian Standard Time", L"US", 21.31, -157.86 },
    { L"Houston, USA", L"Central Standard Time", L"US", 29.76, -95.37 },
    { L"Hyderabad, India", L"India Standard Time", L"IN", 17.39, 78.49 },
    { L"Irkutsk, Russia", L"North Asia East Standard Time", L"", 52.29, 104.28 },
    { L"Istanbul, Türkiye", L"Turkey Standard Time", L"", 41.01, 28.98 },
    { L"Jakarta, Indonesia", L"SE Asia Standard Time", L"", -6.21, 106.85 },
    { L"Jayapura, Indonesia", L"Tokyo Standard Time", L"", -2.53, 140.72 },
    { L"Jerusalem, Israel", L"Israel Standard Time", L"", 31.77, 35.21 },
    { L"Johannesburg, South Africa", L"South Africa Standard Time", L"", -26.20, 28.05 },
    { L"Kabul, Afghanistan", L"Afghanistan Standard Time", L"", 34.56, 69.21 },
    { L"Kaliningrad, Russia", L"Kaliningrad Standard Time", L"", 54.71, 20.45 },
    { L"Kamchatka, Russia", L"Russia Time Zone 11", L"", 53.02, 158.65 },
    { L"Karachi, Pakistan", L"Pakistan Standard Time", L"", 24.86, 67.01 },
    { L"Kathmandu, Nepal", L"Nepal Standard Time", L"", 27.72, 85.32 },
    { L"Kolkata, India", L"India Standard Time", L"IN", 22.57, 88.36 },
    { L"Krasnoyarsk, Russia", L"North Asia Standard Time", L"", 56.01, 92.89 },
    { L"Kuala Lumpur, Malaysia", L"Singapore Standard Time", L"", 3.14, 101.69 },
    { L"Kuwait City, Kuwait", L"Arab Standard Time", L"", 29.38, 47.99 },
    { L"Kyiv, Ukraine", L"FLE Standard Time", L"", 50.45, 30.52 },
    { L"Lagos, Nigeria", L"W. Central Africa Standard Time", L"", 6.52, 3.38 },
    { L"Las Vegas, USA", L"Pacific Standard Time", L"US", 36.17, -115.14 },
    { L"Lima, Peru", L"SA Pacific Standard Time", L"", -12.05, -77.04 },
    { L"Lisbon, Portugal", L"GMT Standard Time", L"", 38.72, -9.14 },
    { L"London, UK", L"GMT Standard Time", L"GB", 51.51, -0.13 },
    { L"Los Angeles, USA", L"Pacific Standard Time", L"US", 34.05, -118.24 },
    { L"Madrid, Spain", L"Romance Standard Time", L"ES", 40.42, -3.70 },
    { L"Magadan, Russia", L"Magadan Standard Time", L"", 59.56, 150.80 },
    { L"Makassar, Indonesia", L"Singapore Standard Time", L"", -5.15, 119.43 },
    { L"Manaus, Brazil", L"SA Western Standard Time", L"BR", -3.12, -60.02 },
    { L"Manila, Philippines", L"Singapore Standard Time", L"", 14.60, 120.98 },
    { L"Melbourne, Australia", L"AUS Eastern Standard Time", L"AU", -37.81, 144.96 },
    { L"Mexico City, Mexico", L"Central Standard Time (Mexico)", L"MX", 19.43, -99.13 },
    { L"Miami, USA", L"Eastern Standard Time", L"US", 25.76, -80.19 },
    { L"Milan, Italy", L"W. Europe Standard Time", L"", 45.46, 9.19 },
    { L"Montreal, Canada", L"Eastern Standard Time", L"CA", 45.50, -73.57 },
    { L"Moscow, Russia", L"Russian Standard Time", L"", 55.76, 37.62 },
    { L"Mumbai, India", L"India Standard Time", L"IN", 19.08, 72.88 },
    { L"Munich, Germany", L"W. Europe Standard Time", L"DE", 48.14, 11.58 },
    { L"Muscat, Oman", L"Arabian Standard Time", L"", 23.59, 58.41 },
    { L"Nairobi, Kenya", L"E. Africa Standard Time", L"", -1.29, 36.82 },
    { L"New York, USA", L"Eastern Standard Time", L"US", 40.71, -74.01 },
    { L"Novosibirsk, Russia", L"N. Central Asia Standard Time", L"", 55.01, 82.93 },
    { L"Omsk, Russia", L"Omsk Standard Time", L"", 54.99, 73.37 },
    { L"Osaka, Japan", L"Tokyo Standard Time", L"JP", 34.69, 135.50 },
    { L"Oslo, Norway", L"W. Europe Standard Time", L"", 59.91, 10.75 },
    { L"Paris, France", L"Romance Standard Time", L"FR", 48.86, 2.35 },
    { L"Perth, Australia", L"W. Australia Standard Time", L"AU", -31.95, 115.86 },
    { L"Phoenix, USA", L"US Mountain Standard Time", L"US", 33.45, -112.07 },
    { L"Prague, Czechia", L"Central Europe Standard Time", L"", 50.08, 14.44 },
    { L"Pune, India", L"India Standard Time", L"IN", 18.52, 73.86 },
    { L"Regina, Canada", L"Canada Central Standard Time", L"CA", 50.45, -104.62 },
    { L"Reykjavik, Iceland", L"Greenwich Standard Time", L"", 64.15, -21.94 },
    { L"Rio Branco, Brazil", L"SA Pacific Standard Time", L"BR", -9.97, -67.81 },
    { L"Riyadh, Saudi Arabia", L"Arab Standard Time", L"", 24.71, 46.68 },
    { L"Rome, Italy", L"W. Europe Standard Time", L"", 41.90, 12.50 },
    { L"Samara, Russia", L"Russia Time Zone 3", L"", 53.20, 50.15 },
    { L"San Francisco, USA", L"Pacific Standard Time", L"US", 37.77, -122.42 },
    { L"Santiago, Chile", L"Pacific SA Standard Time", L"", -33.45, -70.67 },
    { L"São Paulo, Brazil", L"E. South America Standard Time", L"BR", -23.55, -46.63 },
    { L"Seattle, USA", L"Pacific Standard Time", L"US", 47.61, -122.33 },
    { L"Seoul, South Korea", L"Korea Standard Time", L"", 37.57, 126.98 },
    { L"Shanghai, China", L"China Standard Time", L"CN", 31.23, 121.47 },
    { L"Shenzhen, China", L"China Standard Time", L"CN", 22.54, 114.06 },
    { L"Singapore", L"Singapore Standard Time", L"SG", 1.35, 103.82 },
    { L"St. John's, Canada", L"Newfoundland Standard Time", L"CA", 47.56, -52.71 },
    { L"Stockholm, Sweden", L"W. Europe Standard Time", L"", 59.33, 18.07 },
    { L"Sydney, Australia", L"AUS Eastern Standard Time", L"AU", -33.87, 151.21 },
    { L"Taipei, Taiwan", L"Taipei Standard Time", L"", 25.03, 121.57 },
    { L"Tehran, Iran", L"Iran Standard Time", L"", 35.69, 51.39 },
    { L"Tel Aviv, Israel", L"Israel Standard Time", L"", 32.09, 34.78 },
    { L"Tijuana, Mexico", L"Pacific Standard Time (Mexico)", L"MX", 32.51, -117.04 },
    { L"Tokyo, Japan", L"Tokyo Standard Time", L"JP", 35.68, 139.69 },
    { L"Toronto, Canada", L"Eastern Standard Time", L"CA", 43.65, -79.38 },
    { L"Vancouver, Canada", L"Pacific Standard Time", L"CA", 49.28, -123.12 },
    { L"Vienna, Austria", L"W. Europe Standard Time", L"", 48.21, 16.37 },
    { L"Vladivostok, Russia", L"Vladivostok Standard Time", L"", 43.12, 131.89 },
    { L"Warsaw, Poland", L"Central European Standard Time", L"", 52.23, 21.01 },
    { L"Washington DC, USA", L"Eastern Standard Time", L"US", 38.91, -77.04 },
    { L"Wellington, New Zealand", L"New Zealand Standard Time", L"NZ", -41.29, 174.78 },
    { L"Winnipeg, Canada", L"Central Standard Time", L"CA", 49.90, -97.14 },
    { L"Yakutsk, Russia", L"Yakutsk Standard Time", L"", 62.03, 129.73 },
    { L"Yangon, Myanmar", L"Myanmar Standard Time", L"", 16.84, 96.17 },
    { L"Yekaterinburg, Russia", L"Ekaterinburg Standard Time", L"", 56.84, 60.61 },
    { L"Zurich, Switzerland", L"W. Europe Standard Time", L"", 47.38, 8.54 },
};

// ---------------------------------------------------------------- small utils
static ULONGLONG FtU(const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static FILETIME UFt(ULONGLONG u) { FILETIME f; f.dwLowDateTime = (DWORD)u; f.dwHighDateTime = (DWORD)(u >> 32); return f; }
static long long DayNum(const SYSTEMTIME& st) {
    SYSTEMTIME d = st; d.wHour = d.wMinute = d.wSecond = d.wMilliseconds = 0; FILETIME f;
    SystemTimeToFileTime(&d, &f); return (long long)(FtU(f) / (TPM * 1440));
}
static std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static std::wstring Upper(std::wstring s) { for (auto& c : s) c = (wchar_t)towupper(c); return s; }
static std::wstring Fmt(const wchar_t* f, ...) {
    wchar_t b[512]; va_list ap; va_start(ap, f); vswprintf(b, 512, f, ap); va_end(ap); return b;
}

typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
static PFN_GetDpiForWindow pGetDpiForWindow = nullptr;
static float DpiOf(HWND h) {
    if (pGetDpiForWindow && h) { UINT d = pGetDpiForWindow(h); if (d) return (float)d; }
    HDC dc = GetDC(nullptr); int d = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc); return (float)d;
}

// ---------------------------------------------------------------- time zones
static bool FindZone(const std::wstring& key, DYNAMIC_TIME_ZONE_INFORMATION& out) {
    for (DWORD i = 0;; ++i) {
        DYNAMIC_TIME_ZONE_INFORMATION d{};
        if (EnumDynamicTimeZoneInformation(i, &d) != ERROR_SUCCESS) break;
        if (_wcsicmp(d.TimeZoneKeyName, key.c_str()) == 0) { out = d; return true; }
    }
    return false;
}
static void ResolveClock(Clock& c) {
    if (_wcsicmp(c.tzid.c_str(), L"Local") == 0) { c.kind = 1; c.valid = true; }
    else if (_wcsicmp(c.tzid.c_str(), L"UTC") == 0) { c.kind = 2; c.valid = true; }
    else { c.kind = 0; c.valid = FindZone(c.tzid, c.dtzi); }
}
static void RefreshLocalZone() {
    DYNAMIC_TIME_ZONE_INFORMATION d{};
    GetDynamicTimeZoneInformation(&d);
    g_localBias = d.Bias; g_localKey = d.TimeZoneKeyName;
    g_localClock.kind = 1; g_localClock.valid = true; g_localClock.tzid = L"Local";
}
static CTime ClockAt(const Clock& c, ULONGLONG utc) {
    CTime r{}; FILETIME ft = UFt(utc); SYSTEMTIME su; FileTimeToSystemTime(&ft, &su);
    bool ok = false; int stdBias = 0;
    if (c.kind == 2) { r.st = su; ok = true; }
    else if (c.kind == 1) { ok = SystemTimeToTzSpecificLocalTime(nullptr, &su, &r.st) != 0; stdBias = g_localBias; }
    else if (c.valid) { ok = SystemTimeToTzSpecificLocalTimeEx(&c.dtzi, &su, &r.st) != 0; stdBias = c.dtzi.Bias; }
    if (!ok) r.st = su;
    FILETIME lf; SystemTimeToFileTime(&r.st, &lf);
    long long diff = (long long)FtU(lf) - (long long)(utc / 10000 * 10000);
    r.offset = (int)llround(diff / 600000000.0);
    r.dst = (c.kind != 2) && ok && (r.offset != -stdBias);
    return r;
}

struct Frame { ULONGLONG utc; CTime local; std::vector<CTime> t; };
static Frame MakeFrame(long long extraMin = 0) {
    FILETIME f; GetSystemTimeAsFileTime(&f);
    Frame fr; fr.utc = (ULONGLONG)((long long)FtU(f) + (g_shift + extraMin) * (long long)TPM);
    fr.local = ClockAt(g_localClock, fr.utc);
    fr.t.reserve(g_clocks.size());
    for (auto& c : g_clocks) fr.t.push_back(ClockAt(c, fr.utc));
    return fr;
}

static std::wstring FmtHM(const SYSTEMTIME& st, std::wstring* ampm = nullptr) {
    if (S.h24) { if (ampm) ampm->clear(); return Fmt(L"%02d:%02d", st.wHour, st.wMinute); }
    int h = st.wHour % 12; if (!h) h = 12;
    if (ampm) *ampm = st.wHour < 12 ? L"AM" : L"PM";
    return Fmt(L"%d:%02d", h, st.wMinute);
}
static std::wstring FmtHMFull(const SYSTEMTIME& st) {
    std::wstring ap; std::wstring s = FmtHM(st, &ap);
    return ap.empty() ? s : s + L" " + ap;
}
static std::wstring FmtDiff(int mins) {
    if (mins == 0) return L"same time";
    int a = std::abs(mins); const wchar_t* sg = mins > 0 ? L"+" : L"−";
    if (a % 60 == 0) return Fmt(L"%ls%dh", sg, a / 60);
    if (a < 60) return Fmt(L"%ls%dm", sg, a);
    return Fmt(L"%ls%dh %dm", sg, a / 60, a % 60);
}
static std::wstring FmtUtc(int off) {
    int a = std::abs(off); return Fmt(L"UTC%ls%02d:%02d", off < 0 ? L"−" : L"+", a / 60, a % 60);
}
static std::wstring FmtShift() {
    long long a = g_shift < 0 ? -g_shift : g_shift; const wchar_t* sg = g_shift < 0 ? L"−" : L"+";
    long long d = a / 1440, h = (a % 1440) / 60, m = a % 60;
    std::wstring s = sg;
    if (d) s += Fmt(L"%lldd ", d);
    if (h) s += Fmt(L"%lldh ", h);
    if (m || (!d && !h)) s += Fmt(L"%lldm", m);
    while (!s.empty() && s.back() == L' ') s.pop_back();
    return s;
}

#include "holidays.inc"
#include "sun.inc"

// ---------------------------------------------------------------- status & calendar helpers
enum { ST_WORK, ST_FREE, ST_SLEEP, ST_HOLIDAY, ST_AWAY };
static const wchar_t* kStatus[] = { L"Working", L"Free", L"Asleep", L"Holiday", L"Away" };
static int WorkStartOf(const Clock& c) { return c.ws >= 0 ? c.ws : S.workStart; }
static int WorkEndOf(const Clock& c) { return c.we >= 0 ? c.we : S.workEnd; }
static std::wstring HolidayCC(const Clock& c) {
    if (c.cc == L"-") return L"";
    if (!c.cc.empty()) return c.cc;
    return c.kind == 1 ? CountryForZone(g_localKey) : CountryForZone(c.tzid);
}
static bool InWorkHours(const Clock& c, int hour) {
    int a = WorkStartOf(c), b = WorkEndOf(c);
    return a < b ? (hour >= a && hour < b) : (hour >= a || hour < b);   // overnight shifts wrap
}
static bool IsAwayOn(const Clock& c, long long day) { return c.awayY && day <= DaysOf(c.awayY, c.awayM, c.awayD); }
static int ClockStatusAt(const Clock& c, const SYSTEMTIME& st, std::wstring* why = nullptr) {
    long long day = DaysOf(st.wYear, st.wMonth, st.wDay);
    if (IsAwayOn(c, day)) {
        if (why) *why = !c.note.empty() ? c.note : Fmt(L"away until %d %ls", c.awayD, kMonths[std::clamp(c.awayM, 1, 12) - 1]);
        return ST_AWAY;
    }
    if (const HolDay* h = HolidayOn(HolidayCC(c), day)) { if (why) *why = h->name; return ST_HOLIDAY; }
    bool off = (c.offMask >> st.wDayOfWeek) & 1;
    if (!off && InWorkHours(c, st.wHour)) return ST_WORK;
    if (st.wHour < 7 || st.wHour >= 23) return ST_SLEEP;
    return ST_FREE;
}
static SYSTEMTIME StOfMin(long long m) { FILETIME f = UFt((ULONGLONG)m * TPM); SYSTEMTIME st; FileTimeToSystemTime(&f, &st); return st; }
static std::wstring DisplayName(const Clock& c) { return c.person.empty() ? c.label : c.person + L" · " + c.label; }
static std::wstring DisplayName(int i) { return DisplayName(g_clocks[i]); }
static std::wstring DaysOffText(const Clock& c) {
    std::wstring s;
    for (int d = 1; d <= 7; d++) { int k = d % 7; if ((c.offMask >> k) & 1) s += (s.empty() ? L"" : L" ") + std::wstring(kDays[k]); }
    return s.empty() ? L"none" : s;
}
static std::wstring DstNoteFor(const Clock& c);    // planning.inc
// what kind of hour this is for a clock: 0 working, 1 daylight, 2 night, 3 day off / holiday / away (daylight)
static int HourKind(const Clock& c, long long localHourStart, int offset) {
    SYSTEMTIME st = StOfMin(localHourStart + 30);
    int status = ClockStatusAt(c, st);
    if (status == ST_WORK) return 0;
    int up = SunUpAt(c, localHourStart + 30 - offset);
    if (up < 0) up = st.wHour >= 7 && st.wHour < 21;
    if (!up) return 2;
    bool off = status == ST_HOLIDAY || status == ST_AWAY || ((c.offMask >> st.wDayOfWeek) & 1);
    return off ? 3 : 1;
}
static bool WorkingAtUtc(const Clock& c, long long u) { return ClockStatusAt(c, ClockAt(c, (ULONGLONG)u * TPM).st) == ST_WORK; }

static int IsoWeek(const SYSTEMTIME& st, int* doy) {
    static const int cum[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    auto leap = [](int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; };
    auto weeks = [](int y) { auto p = [](int v) { return (v + v / 4 - v / 100 + v / 400) % 7; }; return 52 + ((p(y) == 4 || p(y - 1) == 3) ? 1 : 0); };
    int y = st.wYear, d = cum[st.wMonth - 1] + st.wDay + (leap(y) && st.wMonth > 2 ? 1 : 0);
    if (doy) *doy = d;
    int wd = st.wDayOfWeek == 0 ? 7 : st.wDayOfWeek, w = (d - wd + 10) / 7;
    if (w < 1) return weeks(y - 1);
    if (w > weeks(y)) return 1;
    return w;
}

static std::wstring TooltipExtras(const Clock& c, long long u);   // panels.inc: weather, moon, prayer times
static std::wstring ClockTooltip(int i, const Frame& fr) {
    const Clock& c = g_clocks[i]; const CTime& t = fr.t[i];
    std::wstring zone = c.kind == 1 ? g_localKey + L" (this PC)" : c.kind == 2 ? L"Coordinated Universal Time" : (c.valid ? c.tzid : c.tzid + L" (not found)");
    std::wstring s = DisplayName(c) + L"\n";
    int doy = 0, wk = IsoWeek(t.st, &doy);
    s += Fmt(L"%ls, %d %ls %d  ·  week %d, day %d\n", kDaysL[t.st.wDayOfWeek], t.st.wDay, kMonthsL[t.st.wMonth - 1], t.st.wYear, wk, doy);
    std::wstring ap; std::wstring hm = FmtHM(t.st, &ap);
    s += Fmt(L"%ls:%02d%ls%ls   %ls%ls\n", hm.c_str(), t.st.wSecond, ap.empty() ? L"" : L" ", ap.c_str(), FmtUtc(t.offset).c_str(), t.dst ? L" · daylight saving" : L"");
    std::wstring why; int st = ClockStatusAt(c, t.st, &why);
    s += FmtDiff(t.offset - fr.local.offset) + L" from your time  ·  " + kStatus[st] + (why.empty() ? L"" : L" (" + why + L")") + L"\n";
    s += Fmt(L"Works %02d:00–%02d:00, days off: %ls", WorkStartOf(c), WorkEndOf(c) % 24, DaysOffText(c).c_str());
    std::wstring cc = HolidayCC(c); if (!cc.empty()) s += L"  ·  holidays: " + std::wstring(CountryName(cc));
    s += L"\n";
    long long day = DaysOf(t.st.wYear, t.st.wMonth, t.st.wDay), r, sset;
    int k = SunForLocalDay(c, day, r, sset);
    if (k == 0) {
        SYSTEMTIME a = StOfMin(r + t.offset), b = StOfMin(sset + t.offset);
        s += L"Sunrise " + Lower(FmtHMFull(a)) + L"  ·  sunset " + Lower(FmtHMFull(b)) + L"\n";
    } else if (k == 1) s += L"Sun up all day\n"; else if (k == 2) s += L"Sun down all day\n";
    std::wstring dn = DstNoteFor(c); if (!dn.empty()) s += dn + L"\n";
    s += TooltipExtras(c, (long long)(fr.utc / TPM));
    if (!c.note.empty() && st != ST_AWAY) s += L"Note: " + c.note + L"\n";
    s += zone;
    if (g_shift) s += L"\n(shifted " + FmtShift() + L")";
    return s;
}

// ---------------------------------------------------------------- alarms & countdowns
struct Alarm { std::wstring label, tzid; int hh = 9, mm = 0, rep = 0, y = 0, mo = 0, d = 0; bool enabled = true; long long next = -1; int lead = 0; long long firedFor = -1; };
static const int kLeads[] = { 0, 5, 10, 15, 30, 60, 120, 180, 240, 1440 };
static std::vector<Alarm> g_alarms;
static std::wstring g_flash; static ULONGLONG g_flashUntil = 0;
static HICON g_bigIcon = nullptr;
static const wchar_t* kRepeat[] = { L"Once", L"Every day", L"Weekdays (Mon–Fri)", L"Weekends" };
static void SaveAlarms();

static long long NowMin() { FILETIME f; GetSystemTimeAsFileTime(&f); return (long long)(FtU(f) / TPM); }
static Clock ZoneClock(const std::wstring& tz) { Clock c; c.tzid = tz; ResolveClock(c); return c; }
static bool LocalToUtcMin(const Clock& c, SYSTEMTIME lt, long long& out) {
    SYSTEMTIME u{}; BOOL ok = FALSE;
    lt.wSecond = lt.wMilliseconds = 0;
    if (c.kind == 2) { u = lt; ok = TRUE; }
    else if (c.kind == 1) ok = TzSpecificLocalTimeToSystemTime(nullptr, &lt, &u);
    else if (c.valid) ok = TzSpecificLocalTimeToSystemTimeEx(&c.dtzi, &lt, &u);
    if (!ok) return false;
    FILETIME f; if (!SystemTimeToFileTime(&u, &f)) return false;
    out = (long long)(FtU(f) / TPM); return true;
}
static void ComputeNext(Alarm& a, long long after) {
    Clock c = ZoneClock(a.tzid); a.next = -1;
    if (a.y) {
        SYSTEMTIME lt{}; lt.wYear = (WORD)a.y; lt.wMonth = (WORD)a.mo; lt.wDay = (WORD)a.d; lt.wHour = (WORD)a.hh; lt.wMinute = (WORD)a.mm;
        long long u; if (LocalToUtcMin(c, lt, u) && u > after) a.next = u;
        return;
    }
    CTime now = ClockAt(c, (ULONGLONG)(after + 1) * TPM);
    SYSTEMTIME base = now.st; base.wHour = base.wMinute = base.wSecond = base.wMilliseconds = 0;
    FILETIME bf; SystemTimeToFileTime(&base, &bf);
    for (int d = 0; d < 9; d++) {
        FILETIME f = UFt(FtU(bf) + (ULONGLONG)d * 1440 * TPM); SYSTEMTIME lt; FileTimeToSystemTime(&f, &lt);
        bool we = lt.wDayOfWeek == 0 || lt.wDayOfWeek == 6;
        if ((a.rep == 2 && we) || (a.rep == 3 && !we)) continue;
        lt.wHour = (WORD)a.hh; lt.wMinute = (WORD)a.mm;
        long long u; if (LocalToUtcMin(c, lt, u) && u > after) { a.next = u; return; }
    }
}
static std::wstring PlaceName(const std::wstring& tz) {
    for (auto& c : g_clocks) if (_wcsicmp(c.tzid.c_str(), tz.c_str()) == 0) return c.label;
    if (_wcsicmp(tz.c_str(), L"Local") == 0) return L"your time";
    return tz;
}
static std::wstring FmtSpan(long long m) {
    if (m >= 1440) return Fmt(L"%lldd %lldh", m / 1440, (m % 1440) / 60);
    if (m >= 60) return Fmt(L"%lldh %02lldm", m / 60, m % 60);
    return Fmt(L"%lldm", m);
}
static int NextAlarmIdx() {
    int best = -1;
    for (int i = 0; i < (int)g_alarms.size(); i++)
        if (g_alarms[i].enabled && g_alarms[i].next >= 0 && (best < 0 || g_alarms[i].next < g_alarms[best].next)) best = i;
    return best;
}
static std::wstring AlarmWhen(const Alarm& a, bool withDay) {
    SYSTEMTIME st{}; st.wHour = (WORD)a.hh; st.wMinute = (WORD)a.mm;
    std::wstring s;
    if (a.next >= 0) {
        CTime t = ClockAt(ZoneClock(a.tzid), (ULONGLONG)a.next * TPM); st = t.st;
        if (a.y) s = Fmt(L"%ls %d %ls, ", kDays[st.wDayOfWeek], st.wDay, kMonths[st.wMonth - 1]);
        else if (withDay) s = std::wstring(kDays[st.wDayOfWeek]) + L" ";
    } else if (a.y) s = Fmt(L"%d %ls %d, ", a.d, kMonths[std::clamp(a.mo, 1, 12) - 1], a.y);
    return s + FmtHMFull(st) + L" " + PlaceName(a.tzid);
}
static std::wstring AlarmFooterText(int i) {
    const Alarm& a = g_alarms[i]; long long d = a.next - NowMin();
    return a.label + L"  ·  " + AlarmWhen(a, d >= 20 * 60) + L"  ·  " + (d <= 0 ? std::wstring(L"now") : L"in " + FmtSpan(d));
}
static std::wstring AlarmMenuName(int i) {
    const Alarm& a = g_alarms[i];
    std::wstring n = a.label + L"   (" + AlarmWhen(a, false);
    if (!a.enabled) n += L", off"; else if (a.next < 0) n += L", passed"; else if (a.rep) n += std::wstring(L", ") + kRepeat[a.rep];
    n += L")"; std::replace(n.begin(), n.end(), L'&', L'+');
    return n;
}
static float FooterH(float s) { return NextAlarmIdx() >= 0 ? 28 * s : 0; }

static bool g_trayAdded = false;
static void AddTrayIcon();
static bool g_balloonUpdate = false;   // set right after showing the update balloon; any other balloon clears it
static bool g_balloonTravel = false;   // set right after the "new time zone" balloon
static void Balloon(const std::wstring& title, const std::wstring& text) {
    g_balloonUpdate = false; g_balloonTravel = false;
    if (!g_trayAdded) { AddTrayIcon(); if (!S.trayIcon) SetTimer(g_main, 5, 12000, nullptr); }
    if (!g_nid.hWnd) return;
    NOTIFYICONDATAW n = g_nid; n.uFlags = NIF_INFO;
    wcsncpy(n.szInfoTitle, title.c_str(), 63); n.szInfoTitle[63] = 0;
    wcsncpy(n.szInfo, text.c_str(), 255); n.szInfo[255] = 0;
    n.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON | NIIF_NOSOUND; n.hBalloonIcon = g_bigIcon;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

// ---------------------------------------------------------------- global shortcuts (user-configurable)
static std::wstring HotkeyText(WORD hk) {
    BYTE vk = LOBYTE(hk), f = HIBYTE(hk);
    if (!vk) return L"none";
    std::wstring t;
    if (f & HOTKEYF_CONTROL) t += L"Ctrl+";
    if (f & HOTKEYF_ALT) t += L"Alt+";
    if (f & HOTKEYF_SHIFT) t += L"Shift+";
    wchar_t name[64] = L"";
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lp = (LONG)(sc << 16);
    if ((f & HOTKEYF_EXT) || (vk >= VK_PRIOR && vk <= VK_HELP)) lp |= 1 << 24;
    if (vk == VK_SPACE) wcscpy(name, L"Space");
    else if (!GetKeyNameTextW(lp, name, 64)) swprintf(name, 64, L"Key %u", vk);
    return t + name;
}
static std::wstring HotkeyTab(WORD hk) { return S.hotkey && LOBYTE(hk) ? L"\t" + HotkeyText(hk) : L""; }
static void RegisterHotkeys(bool report) {
    for (int id = 1; id <= 3; id++) UnregisterHotKey(g_main, id);
    if (!S.hotkey) return;
    WORD keys[3] = { S.hkShow, S.hkConvert, S.hkPalette };
    const wchar_t* what[3] = { L"show/hide widget", L"convert copied time", L"command palette" };
    std::wstring failed;
    for (int i = 0; i < 3; i++) {
        BYTE vk = LOBYTE(keys[i]), f = HIBYTE(keys[i]); if (!vk) continue;
        UINT mods = MOD_NOREPEAT | ((f & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) | ((f & HOTKEYF_ALT) ? MOD_ALT : 0) | ((f & HOTKEYF_SHIFT) ? MOD_SHIFT : 0);
        if (!RegisterHotKey(g_main, i + 1, mods, vk)) failed += (failed.empty() ? L"" : L", ") + HotkeyText(keys[i]) + L" (" + what[i] + L")";
    }
    if (report && !failed.empty())
        Balloon(L"Shortcut already in use", failed + L" is taken by another app. Pick a different key in Settings → General.");
}
static void PlayAlert() {
    if (!PlaySoundW(L"Notification.Reminder", nullptr, SND_ALIAS | SND_ASYNC | SND_NODEFAULT)) MessageBeep(MB_ICONASTERISK);
}
static void FireAlarm(const Alarm& a) {
    std::wstring body = AlarmWhen(a, false);
    if (a.lead > 0) {   // a "leave by" reminder: the event is still ahead
        body = a.label + L" at " + body + L"  (in " + FmtSpan(std::max(1LL, a.next - NowMin())) + L")";
        CTime local = ClockAt(g_localClock, (ULONGLONG)a.next * TPM);
        if (_wcsicmp(a.tzid.c_str(), L"Local") != 0) body += L"\n= " + FmtHMFull(local.st) + L" your time";
        Balloon(L"\u23f0 Time to go: " + a.label, body); PlayAlert();
        g_flash = L"leave now \u00b7 " + a.label + L" in " + FmtSpan(std::max(1LL, a.next - NowMin()));
        g_flashUntil = GetTickCount64() + 5 * 60 * 1000;
        return;
    }
    if (_wcsicmp(a.tzid.c_str(), L"Local") != 0) {
        CTime local = ClockAt(g_localClock, (ULONGLONG)a.next * TPM);
        body += L"\n= " + FmtHMFull(local.st) + L" your time";
    }
    Balloon(L"⏰ " + a.label, body);
    PlayAlert();
    g_flash = a.label + L" — " + AlarmWhen(a, false);
    g_flashUntil = GetTickCount64() + 5 * 60 * 1000;
}
static void CheckAlarms() {
    long long now = NowMin(); bool changed = false;
    for (auto& a : g_alarms) {
        if (!a.enabled) continue;
        if (a.next < 0) { ComputeNext(a, now); continue; }
        long long trig = a.next - a.lead;
        if (trig <= now && a.firedFor != a.next) {
            if (now - trig <= 60) FireAlarm(a);   // skip alarms missed by more than an hour (PC was off)
            a.firedFor = a.next;
        }
        if (a.next <= now) {
            if (a.rep == 0) { a.enabled = false; a.next = -1; changed = true; }
            else ComputeNext(a, now);
        }
    }
    if (changed) SaveAlarms();
}

// ---------------------------------------------------------------- settings (INI)
static std::wstring IniStr(const wchar_t* sec, const wchar_t* k, const wchar_t* def) {
    wchar_t b[512]; GetPrivateProfileStringW(sec, k, def, b, 512, g_ini.c_str()); return b;
}
static int IniInt(const wchar_t* sec, const wchar_t* k, int def) { return (int)GetPrivateProfileIntW(sec, k, def, g_ini.c_str()); }
static void IniPut(const wchar_t* sec, const wchar_t* k, const std::wstring& v) { WritePrivateProfileStringW(sec, k, v.c_str(), g_ini.c_str()); }
static void IniPutI(const wchar_t* sec, const wchar_t* k, int v) { IniPut(sec, k, std::to_wstring(v)); }
static int KeyIndex(const std::wstring& v, const wchar_t* const* keys, int n, int def) {
    for (int i = 0; i < n; i++) { if (_wcsicmp(v.c_str(), keys[i]) == 0) return i; }
    return def;
}

static void ChooseIniPath() {
    wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring dir = p; dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    std::wstring local = dir + L"WinClock.ini";
    if (GetFileAttributesW(local.c_str()) != INVALID_FILE_ATTRIBUTES) { g_ini = local; return; }
    HANDLE h = CreateFileW(local.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        // UTF-16 LE BOM so Unicode labels survive the round trip
        DWORD wr; const BYTE bom[2] = { 0xFF, 0xFE }; WriteFile(h, bom, 2, &wr, nullptr); CloseHandle(h);
        g_ini = local; return;
    }
    wchar_t ad[MAX_PATH]; GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH);
    std::wstring d2 = std::wstring(ad) + L"\\WinClock"; CreateDirectoryW(d2.c_str(), nullptr);
    g_ini = d2 + L"\\WinClock.ini";
    if (GetFileAttributesW(g_ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h2 = CreateFileW(g_ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h2 != INVALID_HANDLE_VALUE) { DWORD wr; const BYTE bom[2] = { 0xFF, 0xFE }; WriteFile(h2, bom, 2, &wr, nullptr); CloseHandle(h2); }
    }
}

static bool LocaleIs24h() {
    wchar_t f[80] = L""; GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, f, 80);
    return wcschr(f, L'H') != nullptr;
}

static std::wstring ProfileSection() { return S.profile == L"Default" ? std::wstring(L"Clocks") : L"Clocks." + S.profile; }
static void FillFromAlias(Clock& c) {
    if (c.kind != 0) {
        if (c.kind == 1 && std::isnan(c.lat)) {   // the PC's own zone: borrow a city in the same zone
            for (auto& a : kAliases) if (_wcsicmp(a.tz, g_localKey.c_str()) == 0) { c.lat = a.lat; c.lon = a.lon; break; }
        }
        return;
    }
    const Alias* best = nullptr;
    for (auto& a : kAliases) {
        if (_wcsicmp(a.tz, c.tzid.c_str()) != 0) continue;
        std::wstring city = a.city; city = city.substr(0, city.find(L','));
        if (_wcsicmp(city.c_str(), c.label.c_str()) == 0) { best = &a; break; }
        if (!best) best = &a;
    }
    if (!best) return;
    if (std::isnan(c.lat)) { c.lat = best->lat; c.lon = best->lon; }
}
static std::vector<std::wstring> Split(const std::wstring& v, wchar_t sep) {
    std::vector<std::wstring> f; size_t p = 0;
    for (;;) { size_t e = v.find(sep, p); f.push_back(v.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p)); if (e == std::wstring::npos) break; p = e + 1; }
    return f;
}
static void ParseClock(const std::wstring& v, Clock& c) {
    auto f = Split(v, L'|');
    c.label = f[0]; if (f.size() > 1) c.tzid = f[1];
    c.taskbar = f.size() > 2 && f[2] == L"1";
    if (f.size() > 3) c.person = f[3];
    if (f.size() > 5) { c.ws = f[4].empty() ? -1 : std::clamp(_wtoi(f[4].c_str()), 0, 23); c.we = f[5].empty() ? -1 : std::clamp(_wtoi(f[5].c_str()), 1, 24); }
    if (f.size() > 6 && !f[6].empty()) c.offMask = _wtoi(f[6].c_str()) & 0x7F;
    if (f.size() > 7) c.cc = f[7];
    if (f.size() > 9 && !f[8].empty() && !f[9].empty()) { c.lat = _wtof(f[8].c_str()); c.lon = _wtof(f[9].c_str()); }
    if (f.size() > 10) c.note = f[10];
    if (f.size() > 11 && !f[11].empty() && swscanf(f[11].c_str(), L"%d-%d-%d", &c.awayY, &c.awayM, &c.awayD) != 3) c.awayY = 0;
    if (f.size() > 12) c.tray = f[12] == L"1";
}
static std::wstring ClockLine(const Clock& c) {
    auto clean = [](std::wstring x) { std::replace(x.begin(), x.end(), L'|', L'/'); return x; };
    std::wstring v = clean(c.label) + L"|" + c.tzid + L"|" + (c.taskbar ? L"1" : L"0") + L"|" + clean(c.person) + L"|" +
        (c.ws >= 0 ? std::to_wstring(c.ws) : L"") + L"|" + (c.we >= 0 ? std::to_wstring(c.we) : L"") + L"|" + std::to_wstring(c.offMask) + L"|" + c.cc + L"|" +
        (std::isnan(c.lat) ? L"" : Fmt(L"%.3f", c.lat)) + L"|" + (std::isnan(c.lon) ? L"" : Fmt(L"%.3f", c.lon)) + L"|" + clean(c.note) + L"|" +
        (c.awayY ? Fmt(L"%04d-%02d-%02d", c.awayY, c.awayM, c.awayD) : L"") + L"|" + (c.tray ? L"1" : L"0");
    return v;
}
static void LoadClocks() {
    g_clocks.clear();
    std::wstring sec = ProfileSection();
    int n = IniInt(sec.c_str(), L"Count", -1);
    if (n < 0 && sec == L"Clocks") {
        const wchar_t* def[][2] = { {L"Local", L"Local"}, {L"London", L"GMT Standard Time"}, {L"New York", L"Eastern Standard Time"}, {L"Tokyo", L"Tokyo Standard Time"} };
        for (auto& d : def) { Clock c; c.label = d[0]; c.tzid = d[1]; g_clocks.push_back(c); }
    } else {
        for (int i = 1; i <= std::min(n, 50); i++) {
            std::wstring v = IniStr(sec.c_str(), (L"Clock" + std::to_wstring(i)).c_str(), L"");
            if (v.find(L'|') == std::wstring::npos) continue;
            Clock c; ParseClock(v, c); g_clocks.push_back(c);
        }
    }
    for (auto& c : g_clocks) { ResolveClock(c); FillFromAlias(c); }
}
static void LoadSettings() {
    const wchar_t* s = L"Settings";
    S.layout = KeyIndex(IniStr(s, L"Layout", L"list"), kLayoutKey, L_COUNT, L_LIST);
    S.flyLayout = KeyIndex(IniStr(s, L"FlyoutLayout", L"analog"), kLayoutKey, L_COUNT, L_ANALOG);
    S.theme = KeyIndex(IniStr(s, L"Theme", L"terminal"), kThemeKey, T_COUNT, T_ELECTRIC);
    S.opacity = std::clamp(IniInt(s, L"Opacity", 100), 20, 100);
    S.scale = std::clamp(IniInt(s, L"Scale", 100), 50, 250);
    S.h24 = IniInt(s, L"Use24h", LocaleIs24h() ? 1 : 0) != 0;
    S.secs = IniInt(s, L"ShowSeconds", 1) != 0;
    S.date = IniInt(s, L"ShowDate", 1) != 0;
    S.zmode = _wcsicmp(IniStr(s, L"ZOrder", L"desktop").c_str(), L"top") == 0 ? Z_TOP : Z_DESKTOP;
    S.locked = IniInt(s, L"Locked", 0) != 0;
    S.clickThrough = IniInt(s, L"ClickThrough", 0) != 0;
    S.x = IniInt(s, L"X", INT_MIN); S.y = IniInt(s, L"Y", INT_MIN);
    S.visible = IniInt(s, L"Visible", 1) != 0;
    S.tbShow = IniInt(s, L"Taskbar", 1) != 0;
    S.tbRotate = _wcsicmp(IniStr(s, L"TaskbarMode", L"all").c_str(), L"rotate") == 0;
    S.tbLabels = IniInt(s, L"TaskbarLabels", 1) != 0;
    S.tbOffset = IniInt(s, L"TaskbarOffset", -1);
    { std::wstring an = IniStr(s, L"TaskbarAnchor", S.tbOffset >= 0 ? L"custom" : L"right");
      S.tbAnchor = _wcsicmp(an.c_str(), L"left") == 0 ? 1 : _wcsicmp(an.c_str(), L"custom") == 0 ? 2 : 0; }
    S.trayIcon = IniInt(s, L"TrayIcon", 1) != 0;
    S.workStart = std::clamp(IniInt(s, L"WorkStart", 9), 0, 23);
    S.workEnd = std::clamp(IniInt(s, L"WorkEnd", 18), 1, 24);
    S.cols = std::clamp(IniInt(s, L"AnalogColumns", 3), 1, 8);
    S.hotkey = IniInt(s, L"Hotkey", 1) != 0;
    S.hkShow = (WORD)IniInt(s, L"HotkeyShow", 0x0657); S.hkConvert = (WORD)IniInt(s, L"HotkeyConvert", 0x0656);
    S.hkPalette = (WORD)IniInt(s, L"HotkeyPalette", 0x064B);
    S.showMap = IniInt(s, L"ShowMap", 0) != 0; S.weather = IniInt(s, L"Weather", 0) != 0; S.weatherF = IniInt(s, L"WeatherF", 0) != 0;
    S.prayer = IniInt(s, L"PrayerTimes", 0) != 0; S.prayerMethod = IniInt(s, L"PrayerMethod", 0); S.asrHanafi = IniInt(s, L"AsrHanafi", 0) != 0;
    S.focusMin = std::clamp(IniInt(s, L"FocusMinutes", 25), 1, 180); S.breakMin = std::clamp(IniInt(s, L"BreakMinutes", 5), 1, 60); S.longMin = std::clamp(IniInt(s, L"LongBreakMinutes", 15), 1, 90);
    S.markets = IniStr(s, L"Markets", L""); S.lastLocalTz = IniStr(s, L"LastLocalZone", L"");
    S.lang = IniInt(s, L"Language", 0);
    S.cBg = 0xFF000000 | (DWORD)IniInt(s, L"CustomBg", 0x101418); S.cFg = 0xFF000000 | (DWORD)IniInt(s, L"CustomFg", 0xEDEFF5);
    S.cAccent = 0xFF000000 | (DWORD)IniInt(s, L"CustomAccent", 0x34D399); S.cBlue = 0xFF000000 | (DWORD)IniInt(s, L"CustomSecondary", 0x60A5FA);
    S.chime = IniInt(s, L"HourlyChime", 0) != 0;
    S.tbWhich = IniInt(s, L"TaskbarWhich", 0) ? 1 : 0;
    S.tbAllMon = IniInt(s, L"TaskbarAllMonitors", 0) != 0;
    S.trayTime = IniInt(s, L"TrayShowsTime", 0) != 0;
    S.updates = IniInt(s, L"UpdateCheck", 1) != 0;
#ifdef STORE_BUILD
    S.updates = false;   // the Microsoft Store updates the app
#endif   // new key, so it is on for everyone by default
    S.lastUpdDay = IniInt(s, L"LastUpdateCheck", 0);
    S.profile = IniStr(s, L"Profile", L"Default"); if (S.profile.empty()) S.profile = L"Default";
    S.autoProfile = IniInt(s, L"AutoProfile", 0) != 0;
    S.calUrl = IniStr(L"Calendar", L"Url", L"");
    S.calOn = IniInt(L"Calendar", L"Enabled", 0) != 0 && !S.calUrl.empty();
    S.calWarn = std::clamp(IniInt(L"Calendar", L"WarnMinutes", 5), 0, 60);

    LoadClocks();
}

static void SaveSettings() {
    const wchar_t* s = L"Settings";
    IniPut(s, L"Layout", kLayoutKey[S.layout]); IniPut(s, L"FlyoutLayout", kLayoutKey[S.flyLayout]);
    IniPut(s, L"Theme", kThemeKey[S.theme]); IniPutI(s, L"Opacity", S.opacity); IniPutI(s, L"Scale", S.scale);
    IniPutI(s, L"Use24h", S.h24); IniPutI(s, L"ShowSeconds", S.secs); IniPutI(s, L"ShowDate", S.date);
    IniPut(s, L"ZOrder", S.zmode == Z_TOP ? L"top" : L"desktop");
    IniPutI(s, L"Locked", S.locked); IniPutI(s, L"ClickThrough", S.clickThrough);
    if (S.x != INT_MIN) { IniPutI(s, L"X", S.x); IniPutI(s, L"Y", S.y); }
    IniPutI(s, L"Visible", S.visible); IniPutI(s, L"Taskbar", S.tbShow);
    IniPut(s, L"TaskbarMode", S.tbRotate ? L"rotate" : L"all"); IniPutI(s, L"TaskbarLabels", S.tbLabels);
    IniPutI(s, L"TaskbarOffset", S.tbOffset); IniPut(s, L"TaskbarAnchor", S.tbAnchor == 1 ? L"left" : S.tbAnchor == 2 ? L"custom" : L"right"); IniPutI(s, L"TrayIcon", S.trayIcon); IniPutI(s, L"WorkStart", S.workStart); IniPutI(s, L"WorkEnd", S.workEnd);
    IniPutI(s, L"AnalogColumns", S.cols); IniPutI(s, L"Hotkey", S.hotkey); IniPutI(s, L"HotkeyShow", S.hkShow); IniPutI(s, L"HotkeyConvert", S.hkConvert); IniPutI(s, L"HotkeyPalette", S.hkPalette);
    IniPutI(s, L"ShowMap", S.showMap); IniPutI(s, L"Weather", S.weather); IniPutI(s, L"WeatherF", S.weatherF);
    IniPutI(s, L"PrayerTimes", S.prayer); IniPutI(s, L"PrayerMethod", S.prayerMethod); IniPutI(s, L"AsrHanafi", S.asrHanafi);
    IniPutI(s, L"FocusMinutes", S.focusMin); IniPutI(s, L"BreakMinutes", S.breakMin); IniPutI(s, L"LongBreakMinutes", S.longMin);
    IniPut(s, L"Markets", S.markets); IniPut(s, L"LastLocalZone", S.lastLocalTz); IniPutI(s, L"Language", S.lang);
    IniPutI(s, L"CustomBg", (int)(S.cBg & 0xFFFFFF)); IniPutI(s, L"CustomFg", (int)(S.cFg & 0xFFFFFF));
    IniPutI(s, L"CustomAccent", (int)(S.cAccent & 0xFFFFFF)); IniPutI(s, L"CustomSecondary", (int)(S.cBlue & 0xFFFFFF)); IniPutI(s, L"HourlyChime", S.chime);
    IniPutI(s, L"TaskbarWhich", S.tbWhich); IniPutI(s, L"TaskbarAllMonitors", S.tbAllMon); IniPutI(s, L"TrayShowsTime", S.trayTime);
    IniPutI(s, L"UpdateCheck", S.updates); IniPutI(s, L"LastUpdateCheck", S.lastUpdDay);
    IniPut(s, L"Profile", S.profile); IniPutI(s, L"AutoProfile", S.autoProfile);
    IniPut(L"Calendar", L"Url", S.calUrl); IniPutI(L"Calendar", L"Enabled", S.calOn); IniPutI(L"Calendar", L"WarnMinutes", S.calWarn);
}
static void SaveClocks() {
    std::wstring sec = ProfileSection();
    WritePrivateProfileStringW(sec.c_str(), nullptr, nullptr, g_ini.c_str());
    IniPutI(sec.c_str(), L"Count", (int)g_clocks.size());
    for (size_t i = 0; i < g_clocks.size(); i++) IniPut(sec.c_str(), (L"Clock" + std::to_wstring(i + 1)).c_str(), ClockLine(g_clocks[i]));
}

static void LoadAlarms() {
    g_alarms.clear(); int n = IniInt(L"Alarms", L"Count", 0); long long now = NowMin();
    for (int i = 1; i <= std::min(n, 50); i++) {
        std::wstring v = IniStr(L"Alarms", (L"Alarm" + std::to_wstring(i)).c_str(), L"");
        std::vector<std::wstring> f; size_t p = 0;
        for (;;) { size_t e = v.find(L'|', p); f.push_back(v.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p)); if (e == std::wstring::npos) break; p = e + 1; }
        if (f.size() < 5) continue;
        Alarm a; a.label = f[0]; a.tzid = f[1];
        if (swscanf(f[2].c_str(), L"%d:%d", &a.hh, &a.mm) != 2) continue;
        a.hh = std::clamp(a.hh, 0, 23); a.mm = std::clamp(a.mm, 0, 59);
        a.rep = std::clamp(_wtoi(f[3].c_str()), 0, 3); a.enabled = f[4] == L"1";
        if (f.size() > 5 && !f[5].empty() && swscanf(f[5].c_str(), L"%d-%d-%d", &a.y, &a.mo, &a.d) != 3) a.y = a.mo = a.d = 0;
        if (f.size() > 6) a.lead = std::clamp(_wtoi(f[6].c_str()), 0, 2880);
        if (a.enabled) ComputeNext(a, now);
        g_alarms.push_back(a);
    }
}
static void SaveAlarms() {
    WritePrivateProfileStringW(L"Alarms", nullptr, nullptr, g_ini.c_str());
    IniPutI(L"Alarms", L"Count", (int)g_alarms.size());
    for (size_t i = 0; i < g_alarms.size(); i++) {
        const Alarm& a = g_alarms[i]; std::wstring lab = a.label; std::replace(lab.begin(), lab.end(), L'|', L'/');
        std::wstring v = lab + L"|" + a.tzid + Fmt(L"|%02d:%02d|%d|%d|", a.hh, a.mm, a.rep, a.enabled ? 1 : 0) + (a.y ? Fmt(L"%04d-%02d-%02d", a.y, a.mo, a.d) : std::wstring()) + Fmt(L"|%d", a.lead);
        IniPut(L"Alarms", (L"Alarm" + std::to_wstring(i + 1)).c_str(), v);
    }
}

static void TrimMemory() { SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1); }
static void SetDlgIcon(HWND d);
static bool ParseDate(const std::wstring& s, int& y, int& mo, int& d);

#include "when.inc"
#include "planning.inc"
#include "net.inc"

// ---------------------------------------------------------------- drawing primitives
struct FontE { int fam; int key; G::Font* f; float ascent; };
static std::vector<FontE*> g_fonts;
static G::FontFamily* g_fam[8] = {};
static int g_famStyle[8] = {};
static std::wstring g_famName[8];
static float g_optScale = 1.0f;              // current zoom x DPI scale, used to pick optical sizes
static G::StringFormat* g_fmt = nullptr;     // typographic, no wrap
static G::StringFormat* g_fmtEll = nullptr;  // typographic, ellipsis

struct FamTry { const wchar_t* name; bool bold; };
static void LoadFam(int i, std::initializer_list<FamTry> tries) {
    for (auto& t : tries) {
        auto f = new G::FontFamily(t.name);
        if (f->GetLastStatus() == G::Ok && f->IsAvailable()) { g_fam[i] = f; g_famStyle[i] = t.bold ? G::FontStyleBold : G::FontStyleRegular; g_famName[i] = t.name; return; }
        delete f;
    }
    g_fam[i] = G::FontFamily::GenericSansSerif()->Clone(); g_famStyle[i] = G::FontStyleRegular; g_famName[i] = L"(generic)";
}
static void InitFonts() {
    // Words use the system font, Segoe UI Variable, which ships optical sizes (Small / Text / Display) the way SF Pro does.
    // Times and numbers use Cascadia Mono: tabular digits that don't jitter as they tick, and the terminal look.
    if (S.lang == 1) {   // Hindi: Devanagari needs Nirmala UI everywhere (GDI+ doesn't fall back between fonts)
        for (int i = 0; i < 8; i++) LoadFam(i, { { (i % 2) ? L"Nirmala UI Semibold" : L"Nirmala UI", false }, { L"Nirmala UI", (i % 2) != 0 }, { L"Segoe UI", false } });
    } else {
    LoadFam(0, { { L"Segoe UI Variable Small", false }, { L"Segoe UI", false } });
    LoadFam(1, { { L"Segoe UI Variable Small Semibold", false }, { L"Segoe UI Semibold", false }, { L"Segoe UI", true } });
    LoadFam(2, { { L"Segoe UI Variable Text", false }, { L"Segoe UI", false } });
    LoadFam(3, { { L"Segoe UI Variable Text Semibold", false }, { L"Segoe UI Semibold", false }, { L"Segoe UI", true } });
    LoadFam(4, { { L"Segoe UI Variable Display", false }, { L"Segoe UI", false } });
    LoadFam(5, { { L"Segoe UI Variable Display Semibold", false }, { L"Segoe UI Semibold", false }, { L"Segoe UI", true } });
    LoadFam(6, { { L"Cascadia Mono", false }, { L"Consolas", false } });
    LoadFam(7, { { L"Cascadia Mono SemiBold", false }, { L"Consolas", true } });
    }
    g_fmt = G::StringFormat::GenericTypographic()->Clone();
    g_fmt->SetFormatFlags(G::StringFormatFlagsNoWrap | G::StringFormatFlagsMeasureTrailingSpaces | G::StringFormatFlagsNoClip);
    g_fmtEll = G::StringFormat::GenericTypographic()->Clone();
    g_fmtEll->SetFormatFlags(G::StringFormatFlagsNoWrap);
    g_fmtEll->SetTrimming(G::StringTrimmingEllipsisCharacter);
}
static void FontsGC() { if (g_fonts.size() > 60) { for (auto e : g_fonts) { delete e->f; delete e; } g_fonts.clear(); } }
static void FreeFonts() {
    for (auto e : g_fonts) { delete e->f; delete e; } g_fonts.clear();
    for (auto& f : g_fam) { delete f; f = nullptr; }
    delete g_fmt; delete g_fmtEll; g_fmt = g_fmtEll = nullptr;
}
// FR/FS: system font regular/semibold (optical size picked from the logical size); FM/FMS: monospace for data
enum { FR = 0, FL = 1, FS = 2, FM = 3, FMS = 4 };
static FontE* F(int fam, float px) {
    int idx;
    if (fam == FM) idx = 6;
    else if (fam == FMS) idx = 7;
    else {
        float logical = px / std::max(0.5f, g_optScale);
        int opt = logical < 12.5f ? 0 : logical < 20.f ? 1 : 2;
        idx = opt * 2 + (fam == FS ? 1 : 0);
    }
    int key = (int)lroundf(px * 10);
    for (auto e : g_fonts) if (e->fam == idx && e->key == key) return e;
    float sz = key / 10.f; int st = g_famStyle[idx];
    auto e = new FontE{ idx, key, new G::Font(g_fam[idx], sz, st, G::UnitPixel), 0 };
    e->ascent = sz * g_fam[idx]->GetCellAscent(st) / (float)g_fam[idx]->GetEmHeight(st);
    g_fonts.push_back(e); return e;
}
static float TW(G::Graphics& g, const std::wstring& s, FontE* f) {
    if (s.empty()) return 0;
    G::RectF r;
    g.MeasureString(s.c_str(), (INT)s.size(), f->f, G::PointF(0, 0), g_fmt, &r); return r.Width;
}
enum { AL, AR, AC };
static void DT(G::Graphics& g, const std::wstring& s, FontE* f, float x, float base, DWORD col, int al = AL, float maxW = 0, bool shadow = false) {
    if (s.empty()) return;
    float y = base - f->ascent;
    float w = maxW > 0 ? maxW : TW(g, s, f) + 1;
    float lx = al == AL ? x : al == AR ? x - w : x - w / 2;
    G::StringFormat* fmt = maxW > 0 ? g_fmtEll : g_fmt;
    fmt->SetAlignment(al == AL ? G::StringAlignmentNear : al == AR ? G::StringAlignmentFar : G::StringAlignmentCenter);
    G::RectF rc(lx, y, w, f->f->GetSize() * 1.6f);
    if (shadow) {
        G::SolidBrush sb(G::Color(0x90000000)); G::RectF r2 = rc; r2.X += 1; r2.Y += 1;
        g.DrawString(s.c_str(), (INT)s.size(), f->f, r2, fmt, &sb);
    }
    G::SolidBrush b{ G::Color(col) };
    g.DrawString(s.c_str(), (INT)s.size(), f->f, rc, fmt, &b);
    fmt->SetAlignment(G::StringAlignmentNear);
}
static void RoundRect(G::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = std::min(r * 2, std::min(w, h)); if (d <= 0.5f) { p.AddRectangle(G::RectF(x, y, w, h)); return; }
    p.AddArc(x, y, d, d, 180, 90); p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90); p.AddArc(x, y + h - d, d, d, 90, 90); p.CloseFigure();
}
static void FillRR(G::Graphics& g, float x, float y, float w, float h, float r, DWORD col) {
    G::GraphicsPath p; RoundRect(p, x, y, w, h, r); G::SolidBrush b{ G::Color(col) }; g.FillPath(&b, &p);
}
// Sun for daytime, coral sun for dusk/dawn, lavender crescent moon for night.
static void DayIcon(G::Graphics& g, float cx, float cy, float r, int hour) {
    bool day = hour >= 7 && hour < 18, dusk = hour == 6 || hour == 18 || hour == 19;
    if (day || dusk) {
        G::SolidBrush b{ G::Color(day ? PINK : 0xFFFF9E7A) };
        g.FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r);
        G::SolidBrush h{ G::Color(A(SNOW, 0x70)) };
        g.FillEllipse(&h, cx - r * 0.55f, cy - r * 0.55f, r * 0.7f, r * 0.7f);
    } else {
        G::GraphicsPath a; a.AddEllipse(cx - r, cy - r, 2 * r, 2 * r);
        G::GraphicsPath b; b.AddEllipse(cx - r * 0.25f, cy - r * 1.35f, 2 * r, 2 * r);
        G::Region rg(&a); rg.Exclude(&b);
        G::SolidBrush br{ G::Color(ICE) }; g.FillRegion(&br, &rg);
    }
}

static void DrawBell(G::Graphics& g, float cx, float cy, float r, DWORD col) {
    G::Pen p{ G::Color(col), std::max(1.2f, r * 0.28f) }; p.SetStartCap(G::LineCapRound); p.SetEndCap(G::LineCapRound);
    g.DrawEllipse(&p, cx - r, cy - r, 2 * r, 2 * r);
    g.DrawLine(&p, cx, cy, cx, cy - r * 0.55f); g.DrawLine(&p, cx, cy, cx + r * 0.45f, cy + r * 0.2f);
    g.DrawLine(&p, cx - r * 0.95f, cy - r * 1.05f, cx - r * 0.45f, cy - r * 1.45f);
    g.DrawLine(&p, cx + r * 0.95f, cy - r * 1.05f, cx + r * 0.45f, cy - r * 1.45f);
}

// ---------------------------------------------------------------- surfaces
static void EnsureSurface(Surface& s, int w, int h) {
    if (s.bmp && s.w == w && s.h == h) return;
    if (s.dc) { SelectObject(s.dc, s.old); DeleteObject(s.bmp); DeleteDC(s.dc); s = Surface(); }
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HDC sdc = GetDC(nullptr);
    s.bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &s.bits, nullptr, 0);
    s.dc = CreateCompatibleDC(sdc); ReleaseDC(nullptr, sdc);
    s.old = (HBITMAP)SelectObject(s.dc, s.bmp); s.w = w; s.h = h;
}
static void FreeSurface(Surface& s) { if (s.dc) { SelectObject(s.dc, s.old); DeleteObject(s.bmp); DeleteDC(s.dc); } s = Surface(); }
static void Present(HWND hwnd, Surface& s, BYTE alpha, POINT* pos) {
    SIZE sz{ s.w, s.h }; POINT src{ 0, 0 }; BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, nullptr, pos, &sz, s.dc, &src, 0, &bf, ULW_ALPHA);
}
// when the widget grows (map, markets, cards), slide it back inside the screen instead of running off the edge
static void KeepOnScreen(HWND h) {
    if (!h || !IsWindowVisible(h) || g_widget.dragging) return;
    RECT r; GetWindowRect(h, &r);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork; int x = r.left, y = r.top, w = r.right - r.left, hh = r.bottom - r.top;
    if (x + w > wa.right) x = wa.right - w; if (y + hh > wa.bottom) y = wa.bottom - hh;
    if (x < wa.left) x = wa.left; if (y < wa.top) y = wa.top;
    if (x != r.left || y != r.top) SetWindowPos(h, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void SetupGraphics(G::Graphics& g) {
    g.SetSmoothingMode(G::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(G::TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(G::PixelOffsetModeHalf);
    g.SetCompositingQuality(G::CompositingQualityHighSpeed);
}

// ---------------------------------------------------------------- tooltip helper
static void TipCreate(Tip& t, HWND owner) {
    t.owner = owner;
    t.tt = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                           0, 0, 0, 0, owner, nullptr, g_inst, nullptr);
    TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti); ti.uFlags = TTF_TRACK | TTF_ABSOLUTE; ti.hwnd = owner; ti.uId = 1; ti.lpszText = (LPWSTR)L"";
    SendMessageW(t.tt, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(t.tt, TTM_SETMAXTIPWIDTH, 0, 420);
}
static void TipShow(Tip& t, const std::wstring& text) {
    if (!t.tt) return;
    t.text = text;
    TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti); ti.hwnd = t.owner; ti.uId = 1; ti.lpszText = (LPWSTR)t.text.c_str();
    SendMessageW(t.tt, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
    POINT pt; GetCursorPos(&pt);
    SendMessageW(t.tt, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x + 16, pt.y + 20));
    if (!t.shown) { SendMessageW(t.tt, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti); t.shown = true; }
}
static void TipHide(Tip& t) {
    if (!t.tt || !t.shown) return;
    TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti); ti.hwnd = t.owner; ti.uId = 1;
    SendMessageW(t.tt, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti); t.shown = false;
}

// ---------------------------------------------------------------- widget layouts (terminal design)
struct Ctx { G::Graphics& g; float s; ThemeC th; Frame& fr; View& v; float W, H; int layout; float x0, x1; };

static bool LayoutHasSeconds(int layout) { return S.secs && !g_shift && (layout == L_LIST || layout == L_ANALOG); }

// chrome geometry in unscaled pixels
static const float kM = 10, kTitle = 16, kTop = 38, kStatusH = 24, kBot = 12 + 10 + 24 + 10, kSide = 24;
struct Col { const wchar_t* name; float w; };
static const Col kCols[] = { { L"ID", 42 }, { L"CITY / PERSON", 170 }, { L"TIME", 138 }, { L"STATE", 86 }, { L"OFFSET", 104 }, { L"DAY", 88 } };
static bool WeatherShown();   // panels.inc
// the city column grows to fit the longest name (plus the weather, when it's on), so names aren't cut off
static float ColW(int k) {
    if (k != 1) return kCols[k].w;
    float longest = 0;
    HDC sdc = GetDC(nullptr);
    { G::Graphics mg(sdc); float keep = g_optScale; g_optScale = 1;
      for (size_t i = 0; i < g_clocks.size(); i++) longest = std::max(longest, TW(mg, DisplayName((int)i), F(FS, 14.5f)));
      g_optScale = keep; }
    ReleaseDC(nullptr, sdc);
    float w = 10 + 12 + longest + 14 + (WeatherShown() ? 52.f : 0.f);
    return std::clamp(w, kCols[1].w, 340.f);
}
static const wchar_t* kLayoutTitle[] = { L"world clocks", L"analog", L"timeline", L"world clocks", L"" };

static void ContentSize(int layout, int n, float& cw, float& ch) {
    n = std::max(n, 1);
    switch (layout) {
    case L_ANALOG: { int cols = std::max(1, std::min(S.cols, n)); int rows = (n + cols - 1) / cols; cw = cols * 148.f; ch = rows * 212.f; break; }
    case L_TIMELINE: cw = 640; ch = 28 + n * 42.f; break;
    case L_COMPACT: cw = 320; ch = n * 31.f; break;
    case L_STRIP: cw = 0; ch = 0; break;
    default: cw = 0; for (int k = 0; k < 6; k++) cw += ColW(k); ch = 28 + n * 38.f; break;
    }
}
static float PinsH() { int np = std::min((int)g_pins.size(), 4); return np ? 10.f + 26.f * np : 0.f; }
static float ExtrasH(float cw);   // panels.inc
static void ComputeSize(int layout, int n, float s, float& W, float& H) {
    float cw, ch; ContentSize(layout, n, cw, ch);
    W = std::max((cw + 2 * kSide) * s, 340 * s);
    H = (kTop + ch + ExtrasH(W / s - 2 * kSide) + kBot) * s;
}

static std::wstring OffShort(int mins) {
    if (mins == 0) return L"±0";
    int a = std::abs(mins); std::wstring sg = mins > 0 ? L"+" : L"−";
    if (a % 60 == 0) return sg + std::to_wstring(a / 60) + L"h";
    if (a < 60) return sg + std::to_wstring(a) + L"m";
    return sg + Fmt(L"%dh%02d", a / 60, a % 60);
}
static std::wstring DayTag(const CTime& t, const Frame& fr) {
    long long dd = DayNum(t.st) - DayNum(fr.local.st);
    return dd > 0 ? L" +1d" : dd < 0 ? L" −1d" : L"";
}
static DWORD StatusColor(const ThemeC& th, int st) { return st == ST_WORK ? th.accent : st == ST_FREE ? th.blue : th.dim; }
static std::wstring OffCore(int i, const Frame& fr) { return g_clocks[i].kind == 1 ? std::wstring(L"local") : OffShort(fr.t[i].offset - fr.local.offset); }
// a loud pill when a city is on another day ("+1d" = tomorrow there)
static float DayPill(G::Graphics& g, const ThemeC& th, int dd, float x, float base, float s, bool alignRight = false) {
    if (!dd) return 0;
    std::wstring t = dd > 0 ? L"+1d" : L"\u22121d";
    FontE* f = F(FMS, 10.5f * s); float w = TW(g, t, f) + 10 * s, h = 15 * s;
    float x0 = alignRight ? x - w : x;
    FillRR(g, x0, base - 11.5f * s, w, h, 4 * s, dd > 0 ? th.accent : th.blue);
    DT(g, t, f, x0 + w / 2, base - 0.5f * s, th.bg | 0xFF000000, AC);
    return w;
}
static void SafeDot(G::Graphics& g, const ThemeC& th, int st, float cx, float cy, float s) {
    float r = 3.6f * s;
    if (st == ST_WORK) { G::SolidBrush b{ G::Color(th.accent) }; g.FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r); }
    else { G::Pen p{ G::Color(st == ST_FREE ? th.blue : A(th.dim, 0xA0)), 1.4f * s }; g.DrawEllipse(&p, cx - r, cy - r, 2 * r, 2 * r); }
}
static std::wstring TimeLower(const SYSTEMTIME& st) { return Lower(FmtHMFull(st)); }
static std::wstring OffFor(int i, const Frame& fr) {
    return (g_clocks[i].kind == 1 ? std::wstring(L"local") : OffShort(fr.t[i].offset - fr.local.offset)) + DayTag(fr.t[i], fr);
}

static void DrawEmpty(Ctx& c, float y) {
    DT(c.g, L"$ no clocks yet — right-click → add clock", F(FR, 13 * c.s), c.x0, y + 22 * c.s, c.th.dim, AL, c.x1 - c.x0, c.th.shadow);
}
static void DotFill(Ctx& c, float x, float y, float w, float h, int alpha) {
    G::HatchBrush hb(G::HatchStyle50Percent, G::Color(A(c.th.fg, alpha)), G::Color(0));
    c.g.FillRectangle(&hb, x, y, w, h);
}

#include "panels.inc"

// ---- table (list) layout
static void DrawList(Ctx& c, float top) {
    float s = c.s, x0 = c.x0, x1 = c.x1, hh = 28 * s, rh = 38 * s;
    float colw[6]; for (int k = 0; k < 6; k++) colw[k] = ColW(k);
    float base = 0; for (int k = 0; k < 6; k++) base += colw[k] * s;
    float extra = std::max(0.f, (x1 - x0) - base);
    float cx[7]; float x = x0;
    for (int k = 0; k < 6; k++) { cx[k] = x; x += colw[k] * s + (k == 1 ? extra : 0); }
    cx[6] = x1;
    { G::SolidBrush hb{ G::Color(c.th.blue) }; c.g.FillRectangle(&hb, x0, top, x1 - x0, hh); }
    FontE* fh = F(FMS, 11.5f * s);
    for (int k = 0; k < 6; k++) DT(c.g, T(kCols[k].name), fh, cx[k] + 10 * s, top + 18.5f * s, c.th.headFg);
    { G::Pen dp{ G::Color(A(c.th.headFg, 0x45)), 1 }; c.g.DrawLine(&dp, cx[3] - 0.5f, top, cx[3] - 0.5f, top + hh); }
    int n = (int)g_clocks.size(); if (!n) { DrawEmpty(c, top + hh); return; }
    for (int i = 0; i < n; i++) {
        float y = top + hh + i * rh, b = y + 25 * s, p = 10 * s; const CTime& t = c.fr.t[i]; int st = ClockStatusAt(g_clocks[i], t.st);
        c.v.hits.push_back({ { (LONG)x0, (LONG)y, (LONG)x1, (LONG)(y + rh) }, i });
        if (c.v.hover == i) { G::SolidBrush hv{ G::Color(c.th.hover) }; c.g.FillRectangle(&hv, x0, y, x1 - x0, rh); }
        DT(c.g, Fmt(L"%03d", i + 1), F(FM, 12.5f * s), cx[0] + p, b, c.th.dim, AL, 0, c.th.shadow);
        SafeDot(c.g, c.th, st, cx[1] + p + 3 * s, b - 5 * s, s);
        {
            std::wstring nm = DisplayName(i); FontE* fn = F(FS, 14.5f * s); float room = cx[2] - cx[1] - p - 18 * s, nw = std::min(room, TW(c.g, nm, fn));
            if (WeatherFor(g_clocks[i])) { room -= 46 * s; nw = std::min(room, nw); }
            DT(c.g, nm, fn, cx[1] + p + 12 * s, b, c.th.fg, AL, room, c.th.shadow);
            std::wstring why; int s2 = ClockStatusAt(g_clocks[i], t.st, &why);
            std::wstring note = (s2 == ST_HOLIDAY || s2 == ST_AWAY) ? why : g_clocks[i].note;
            float wxW = 0;
            if (const Weather* wx = WeatherFor(g_clocks[i])) {   // small icon + temperature at the right of the city column
                std::wstring tt = WxTemp(*wx); FontE* fw = F(FM, 12 * s); float tw = TW(c.g, tt, fw);
                float rx = cx[2] - p; wxW = tw + 22 * s;
                DT(c.g, tt, fw, rx, b, c.th.dim, AR, 0, c.th.shadow);
                DrawWxIcon(c.g, wx->code, SunUpAt(g_clocks[i], (long long)(c.fr.utc / TPM)) == 0, rx - tw - 10 * s, b - 4.5f * s, 6.5f * s, c.th.dim, c.th.accent);
            }
            if (!note.empty() && room - nw - wxW > 30 * s) DT(c.g, L" \u00b7 " + note, F(FR, 12 * s), cx[1] + p + 12 * s + nw, b, c.th.dim, AL, room - nw - wxW, c.th.shadow);
        }
        std::wstring ap, hm = FmtHM(t.st, &ap);
        FontE* ftm = F(FMS, 18 * s); FontE* fsm = F(FM, 12 * s); float tx = cx[2] + p;
        DT(c.g, hm, ftm, tx, b + 0.5f * s, c.th.fg, AL, 0, c.th.shadow); tx += TW(c.g, hm, ftm);
        if (S.secs && !g_shift) { std::wstring sc = Fmt(L":%02d", t.st.wSecond); DT(c.g, sc, fsm, tx, b, c.th.dim, AL, 0, c.th.shadow); tx += TW(c.g, sc, fsm); }
        if (!ap.empty()) DT(c.g, Lower(ap), fsm, tx + 4 * s, b, c.th.accent, AL, 0, c.th.shadow);
        DT(c.g, Lower(T(kStatus[st])), F(FS, 13 * s), cx[3] + p, b, StatusColor(c.th, st), AL, 0, c.th.shadow);
        { std::wstring oc = OffCore(i, c.fr); FontE* fo = F(FM, 13 * s); float ow = TW(c.g, oc, fo);
          DT(c.g, oc, fo, cx[4] + p, b, c.th.dim, AL, cx[5] - cx[4] - p, c.th.shadow);
          DayPill(c.g, c.th, (int)(DayNum(t.st) - DayNum(c.fr.local.st)), cx[4] + p + ow + 6 * s, b, s); }
        float bw = cx[6] - cx[5] - p - 2 * s, frac = (t.st.wHour * 60 + t.st.wMinute) / 1440.f;
        DotFill(c, cx[5] + p, y + 12.5f * s, bw, 13 * s, 0x40);
        G::SolidBrush fb{ G::Color(st == 0 ? c.th.accent : c.th.blue) };
        c.g.FillRectangle(&fb, cx[5] + p, y + 12.5f * s, std::max(1.f, bw * frac), 13 * s);
    }
}

// ---- analog layout
static void DrawFace(Ctx& c, float cx, float cy, float r, const SYSTEMTIME& st) {
    float s = c.s; bool day = st.wHour >= 7 && st.wHour < 19;
    G::SolidBrush fb{ G::Color(c.th.face) }; c.g.FillEllipse(&fb, cx - r, cy - r, 2 * r, 2 * r);
    G::Pen ring{ G::Color(day ? c.th.blue : A(c.th.dim, 0x90)), 2.2f * s }; c.g.DrawEllipse(&ring, cx - r, cy - r, 2 * r, 2 * r);
    for (int i = 0; i < 60; i++) {
        bool hourTick = i % 5 == 0, quarter = i % 15 == 0; if (!hourTick && r < 40 * s) continue;
        float a = i * 6 * 3.14159265f / 180, len = quarter ? r * 0.16f : hourTick ? r * 0.09f : r * 0.035f, r1 = r * 0.88f, r0 = r1 - len;
        G::Pen p{ G::Color(quarter ? c.th.blue : hourTick ? A(c.th.fg, 0xB0) : A(c.th.dim, 0x70)), quarter ? 2.8f * s : hourTick ? 1.5f * s : 1.0f * s };
        c.g.DrawLine(&p, cx + sinf(a) * r0, cy - cosf(a) * r0, cx + sinf(a) * r1, cy - cosf(a) * r1);
    }
    DT(c.g, Lower(kDays[st.wDayOfWeek]) + L" " + std::to_wstring(st.wDay), F(FM, 10 * s), cx, cy + r * 0.5f, c.th.dim, AC);
    auto handf = [&](float deg, float len, float tail, float w, DWORD col) {
        float a = deg * 3.14159265f / 180; G::Pen p{ G::Color(col), w };
        p.SetStartCap(G::LineCapRound); p.SetEndCap(G::LineCapRound);
        c.g.DrawLine(&p, cx - sinf(a) * tail, cy + cosf(a) * tail, cx + sinf(a) * len, cy - cosf(a) * len);
    };
    float hr = (st.wHour % 12) + st.wMinute / 60.f, mn = st.wMinute + st.wSecond / 60.f;
    handf(hr * 30, r * 0.50f, r * 0.08f, 4.0f * s, c.th.fg);
    handf(mn * 6, r * 0.74f, r * 0.08f, 2.6f * s, c.th.fg);
    bool sec = S.secs && !g_shift;
    if (sec) handf(st.wSecond * 6.f, r * 0.84f, r * 0.18f, 1.3f * s, c.th.accent);
    G::SolidBrush cb{ G::Color(c.th.accent) }; c.g.FillEllipse(&cb, cx - 3.4f * s, cy - 3.4f * s, 6.8f * s, 6.8f * s);
    G::SolidBrush cd{ G::Color(c.th.face) }; c.g.FillEllipse(&cd, cx - 1.3f * s, cy - 1.3f * s, 2.6f * s, 2.6f * s);
}
static void DrawAnalog(Ctx& c, float top) {
    float s = c.s, tw = 148 * s, th = 212 * s;
    int n = (int)g_clocks.size(); if (!n) { DrawEmpty(c, top); return; }
    int cols = std::max(1, std::min(S.cols, n));
    float gx = c.x0 + std::max(0.f, ((c.x1 - c.x0) - cols * tw) / 2);
    for (int i = 0; i < n; i++) {
        float x = gx + (i % cols) * tw, y = top + (i / cols) * th; const CTime& t = c.fr.t[i]; int st = ClockStatusAt(g_clocks[i], t.st);
        c.v.hits.push_back({ { (LONG)x, (LONG)y, (LONG)(x + tw), (LONG)(y + th) }, i });
        if (c.v.hover == i) { G::SolidBrush hv{ G::Color(c.th.hover) }; c.g.FillRectangle(&hv, x + 4 * s, y, tw - 8 * s, th - 6 * s); }
        float r = 50 * s, cx = x + tw / 2, cy = y + 8 * s + r;
        DrawFace(c, cx, cy, r, t.st);
        float b = cy + r + 22 * s;
        DT(c.g, DisplayName(i), F(FS, 15 * s), cx, b, c.th.fg, AC, tw - 12 * s, c.th.shadow);
        DT(c.g, TimeLower(t.st), F(FM, 14 * s), cx, b + 21 * s, c.th.fg, AC, 0, c.th.shadow);
        std::wstring sw = Lower(T(kStatus[st])), ow = L" · " + OffFor(i, c.fr);
        FontE* f3 = F(FR, 12.5f * s); float w1 = TW(c.g, sw, f3), w2 = TW(c.g, ow, f3), sx = cx - (w1 + w2) / 2;
        DT(c.g, sw, f3, sx, b + 40 * s, StatusColor(c.th, st), AL, 0, c.th.shadow);
        DT(c.g, ow, f3, sx + w1, b + 40 * s, c.th.dim, AL, 0, c.th.shadow);
    }
}

// ---- compact layout
static void DrawCompact(Ctx& c, float top) {
    float s = c.s, rh = 31 * s, x0 = c.x0, x1 = c.x1;
    int n = (int)g_clocks.size(); if (!n) { DrawEmpty(c, top - 6 * s); return; }
    for (int i = 0; i < n; i++) {
        float y = top + i * rh, b = y + 21 * s; const CTime& t = c.fr.t[i]; int st = ClockStatusAt(g_clocks[i], t.st);
        c.v.hits.push_back({ { (LONG)x0, (LONG)y, (LONG)x1, (LONG)(y + rh) }, i });
        if (c.v.hover == i) { G::SolidBrush hv{ G::Color(c.th.hover) }; c.g.FillRectangle(&hv, x0 - 6 * s, y, x1 - x0 + 12 * s, rh); }
        DT(c.g, L"›", F(FMS, 16 * s), x0, b + 0.5f * s, StatusColor(c.th, st), AL, 0, c.th.shadow);
        std::wstring tm = TimeLower(t.st); FontE* ft = F(FMS, 15.5f * s); float tw = TW(c.g, tm, ft);
        DT(c.g, tm, ft, x1, b, c.th.fg, AR, 0, c.th.shadow);
        std::wstring off = g_clocks[i].kind == 1 ? L"" : OffCore(i, c.fr); FontE* fo = F(FM, 12 * s); float ow = TW(c.g, off, fo);
        DT(c.g, off, fo, x1 - tw - 12 * s, b, c.th.dim, AR, 0, c.th.shadow);
        ow += DayPill(c.g, c.th, (int)(DayNum(t.st) - DayNum(c.fr.local.st)), x1 - tw - ow - 18 * s, b, s, true) + (DayNum(t.st) != DayNum(c.fr.local.st) ? 6 * s : 0);
        DT(c.g, DisplayName(i), F(FS, 14.5f * s), x0 + 16 * s, b, c.th.fg, AL, std::max(10 * s, x1 - tw - ow - 30 * s - x0 - 16 * s), c.th.shadow);
    }
}

// ---- timeline layout
static void DrawTimeline(Ctx& c, float top) {
    float s = c.s, x0 = c.x0, x1 = c.x1, labelW = 150 * s, rowH = 42 * s, head = 28 * s;
    float barX = x0 + labelW, barW = x1 - barX;
    int n = (int)g_clocks.size();
    c.v.tlBar = { (LONG)barX, (LONG)top, (LONG)(barX + barW), (LONG)(top + head + std::max(n, 1) * rowH) };
    float cxNow = barX + barW / 2;
    FontE* fh = F(FM, 11.5f * s);
    DT(c.g, L"−12h", fh, barX, top + 15 * s, c.th.dim, AL, 0, c.th.shadow);
    DT(c.g, L"+12h", fh, barX + barW, top + 15 * s, c.th.dim, AR, 0, c.th.shadow);
    if (n >= 2) DT(c.g, L"■ all working", fh, x0, top + 15 * s, c.th.accent, AL, labelW - 8 * s, c.th.shadow);
    if (c.v.tlHover) {
        Frame hf = MakeFrame(c.v.tlMin);
        float hx = cxNow + c.v.tlMin / 1440.f * barW;
        std::wstring lbl = TimeLower(hf.local.st) + L" (" + (c.v.tlMin >= 0 ? L"+" : L"−") + Fmt(L"%dh%02d", std::abs(c.v.tlMin) / 60, std::abs(c.v.tlMin) % 60) + L")";
        DT(c.g, lbl, F(FMS, 12 * s), std::clamp(hx, barX + 60 * s, barX + barW - 60 * s), top + 15 * s, c.th.fg, AC, 0, c.th.shadow);
    } else {
        DT(c.g, L"now · " + TimeLower(c.fr.local.st), F(FMS, 12 * s), cxNow, top + 15 * s, c.th.accent, AC, 0, c.th.shadow);
    }
    if (!n) { DrawEmpty(c, top + head); return; }
    double utcMin = (double)(c.fr.utc / 10000000ULL) / 60.0;
    float pxPerMin = barW / 1440.f;
    for (int i = 0; i < n; i++) {
        float y = top + head + i * rowH; const CTime& t = c.fr.t[i];
        c.v.hits.push_back({ { (LONG)x0, (LONG)y, (LONG)barX, (LONG)(y + rowH) }, i });
        if (c.v.hover == i && !c.v.tlHover) { G::SolidBrush hv{ G::Color(c.th.hover) }; c.g.FillRectangle(&hv, x0 - 6 * s, y + 2 * s, labelW, rowH - 4 * s); }
        SafeDot(c.g, c.th, ClockStatusAt(g_clocks[i], t.st), x0 + 3.5f * s, y + 14 * s, s);
        DT(c.g, DisplayName(i), F(FS, 14.5f * s), x0 + 11 * s, y + 19 * s, c.th.fg, AL, labelW - 22 * s, c.th.shadow);
        DT(c.g, TimeLower(t.st) + L" " + OffFor(i, c.fr), F(FM, 11.5f * s), x0, y + 35 * s, c.th.dim, AL, labelW - 12 * s, c.th.shadow);
        float by = y + 7 * s, bh = rowH - 14 * s;
        c.g.SetClip(G::RectF(barX, by, barW, bh));
        double localStart = utcMin - 720 + t.offset;
        for (double m = floor(localStart / 60) * 60; ; m += 60) {
            float x = barX + (float)((m - localStart) * pxPerMin);
            if (x >= barX + barW) break;
            float cw = 60 * pxPerMin;
            long long hAbs = (long long)floor(m / 60); int hour = (int)(((hAbs % 24) + 24) % 24);
            long long dayIdx = (long long)floor(m / 1440); int dow = (int)(((dayIdx + 1) % 7 + 7) % 7);   // 1601-01-01 was a Monday
            int kind = HourKind(g_clocks[i], (long long)m, t.offset);   // working / daylight / night (real sunrise & sunset) / day off
            bool work = kind == 0;
            if (work) { G::SolidBrush b{ G::Color(c.th.blue) }; c.g.FillRectangle(&b, x + 0.6f * s, by, cw - 1.2f * s, bh); }
            else if (kind == 1) { G::SolidBrush b{ G::Color(A(c.th.fg, 0x1C)) }; c.g.FillRectangle(&b, x + 0.6f * s, by, cw - 1.2f * s, bh); }
            else if (kind == 3) { G::SolidBrush b{ G::Color(A(c.th.accent, 0x30)) }; c.g.FillRectangle(&b, x + 0.6f * s, by, cw - 1.2f * s, bh); }
            else DotFill(c, x + 0.6f * s, by, cw - 1.2f * s, bh, 0x30);
            if (hour == 0) DT(c.g, Lower(kDays[dow]), F(FMS, 9.5f * s), x + cw / 2, by + bh / 2 + 3.2f * s, work ? c.th.headFg : c.th.accent, AC);
            else if (cw >= 13 * s) {
                int hh = S.h24 ? hour : (hour % 12 == 0 ? 12 : hour % 12);
                DT(c.g, std::to_wstring(hh), F(FM, 10.5f * s), x + cw / 2, by + bh / 2 + 3.4f * s, work ? c.th.headFg : c.th.dim, AC);
            }
        }
        c.g.ResetClip();
    }
    float ly0 = top + head - 3 * s, ly1 = top + head + n * rowH;
    if (n >= 2) {   // pink band where every city is inside working hours
        double s0 = floor((utcMin - 720) / 15) * 15; float runX = -1;
        auto flush = [&](float xEnd) {
            if (runX < 0) return;
            float a0 = std::max(runX, barX), a1 = std::min(xEnd, barX + barW);
            if (a1 > a0) {
                G::SolidBrush ob{ G::Color(A(c.th.accent, 0x40)) }; c.g.FillRectangle(&ob, a0, ly0 + 3 * s, a1 - a0, ly1 - ly0 - 3 * s);
                G::Pen op{ G::Color(c.th.accent), 1.6f * s }; c.g.DrawRectangle(&op, a0, ly0 + 3 * s, a1 - a0, ly1 - ly0 - 3 * s);
            }
            runX = -1;
        };
        for (double m = s0; m < utcMin + 720; m += 15) {
            bool all = true;
            for (int i = 0; i < n && all; i++)
                all = ClockStatusAt(g_clocks[i], StOfMin((long long)(m + c.fr.t[i].offset) + 7)) == ST_WORK;
            float xx = barX + (float)((m - (utcMin - 720)) * pxPerMin);
            if (all) { if (runX < 0) runX = xx; } else flush(xx);
        }
        flush(barX + barW);
    }
    { G::Pen p{ G::Color(c.th.accent), 2 * s }; c.g.DrawLine(&p, cxNow, ly0, cxNow, ly1);
      G::SolidBrush b{ G::Color(c.th.accent) }; c.g.FillRectangle(&b, cxNow - 3.5f * s, ly0 - 3.5f * s, 7 * s, 7 * s); }
    if (c.v.tlHover) {
        float hx = cxNow + c.v.tlMin * pxPerMin;
        G::Pen p{ G::Color(A(c.th.fg, 0xC0)), 1.2f * s }; p.SetDashStyle(G::DashStyleDash);
        c.g.DrawLine(&p, hx, ly0, hx, ly1);
    }
}

// ---- frame: panel, bracket titles, status bar
static void PaintView(View& v) {
    if (!v.hwnd) return;
    int layout = v.fly ? S.flyLayout : S.layout;
    int theme = ResolveTheme(S.theme); if (v.fly && (theme == T_GLASS || theme == T_TRANSPARENT)) theme = T_ELECTRIC;
    float s = (v.fly ? 1.0f : S.scale / 100.f) * DpiOf(v.hwnd) / 96.f * g_textScale;
    g_optScale = s;
    if (!g_shift) g_conv.on = false;
    FontsGC();
    Frame fr = MakeFrame();
    if (layout == L_STRIP) { PaintStrip(v, s, theme, fr); return; }
    float W, H; ComputeSize(layout, (int)g_clocks.size(), s, W, H);
    int w = (int)ceilf(W), h = (int)ceilf(H);
    EnsureSurface(v.surf, w, h);
    v.hits.clear(); v.tlBar = RECT{}; v.flashRc = RECT{};
    {
        G::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)v.surf.bits);
        G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0, 0, 0, 0));
        ThemeC th = GetTheme(theme);
        // crisp ClearType where the panel is opaque; grayscale AA where the desktop shows through
        if ((th.bg >> 24) >= 0xF0 && (v.fly || S.opacity >= 95)) g.SetTextRenderingHint(G::TextRenderingHintClearTypeGridFit);
        { G::GraphicsPath p; RoundRect(p, 0.5f, 0.5f, w - 1.f, h - 1.f, 6 * s); G::SolidBrush b{ G::Color(th.bg) }; g.FillPath(&b, &p);
          if (th.outline) { G::Pen pen{ G::Color(th.outline), 1 }; g.DrawPath(&pen, &p); } }
        float bx0 = kM * s, by0 = kTitle * s, bx1 = w - kM * s, sy = h - (kM + kStatusH) * s, by1 = sy - 10 * s;
        { G::Pen bp{ G::Color(th.border), std::max(1.f, s) }; g.DrawRectangle(&bp, bx0, by0, bx1 - bx0, by1 - by0); }
        auto erase = [&](float x, float y, float ww, float hh) {
            g.SetCompositingMode(G::CompositingModeSourceCopy); g.SetSmoothingMode(G::SmoothingModeNone); g.SetPixelOffsetMode(G::PixelOffsetModeNone);
            G::SolidBrush eb{ G::Color(th.bg) };
            g.FillRectangle(&eb, (INT)floorf(x), (INT)floorf(y), (INT)ceilf(ww), (INT)ceilf(hh) + 1);
            g.SetCompositingMode(G::CompositingModeSourceOver); g.SetSmoothingMode(G::SmoothingModeAntiAlias); g.SetPixelOffsetMode(G::PixelOffsetModeHalf);
        };
        FontE* ft = F(FMS, 13.5f * s);
        std::wstring title = std::wstring(L"[ ") + T(kLayoutTitle[layout]) + L" ]";
        float tx = bx0 + 16 * s, tw = TW(g, title, ft);
        erase(tx - 6 * s, by0 - 4 * s, tw + 12 * s, 8 * s);
        DT(g, title, ft, tx, by0 + 4.5f * s, th.blue, AL, 0, th.shadow);
        std::wstring rt = L"[ " + Lower(Fmt(L"%ls %02d %ls", kDays[fr.local.st.wDayOfWeek], fr.local.st.wDay, kMonths[fr.local.st.wMonth - 1])) + L" ]";
        float rw = TW(g, rt, ft), rx = bx1 - 16 * s - rw;
        if (rx > tx + tw + 20 * s) { erase(rx - 6 * s, by0 - 4 * s, rw + 12 * s, 8 * s); DT(g, rt, ft, rx, by0 + 4.5f * s, th.accent, AL, 0, th.shadow); }

        Ctx c{ g, s, th, fr, v, (float)w, (float)h, layout, kSide * s, w - kSide * s };
        float top = kTop * s;
        switch (layout) {
        case L_ANALOG: DrawAnalog(c, top); break;
        case L_TIMELINE: DrawTimeline(c, top); break;
        case L_COMPACT: DrawCompact(c, top); break;
        default: DrawList(c, top); break;
        }

        // answer cards, timers, markets, map
        float cw0, ch0; ContentSize(layout, (int)g_clocks.size(), cw0, ch0);
        DrawPanels(c, (kTop + ch0) * s);
        // pinned moments
        int np = std::min((int)g_pins.size(), 4);
        if (np) {
            float py = (kTop + ch0 + ExtrasH(w / s - 2 * kSide) - PinsH() + 6) * s, x0 = c.x0, x1 = c.x1;
            G::Pen sp{ G::Color(th.sep), 1 }; g.DrawLine(&sp, x0, py, x1, py);
            for (int k = 0; k < np; k++) {
                const Pin& pn = g_pins[k]; float y = py + 4 * s + k * 26 * s, b = y + 17 * s;
                v.hits.push_back({ { (LONG)x0, (LONG)y, (LONG)x1, (LONG)(y + 26 * s) }, -100 - k });
                if (v.hover == -100 - k) { G::SolidBrush hv{ G::Color(th.hover) }; g.FillRectangle(&hv, x0 - 6 * s, y, x1 - x0 + 12 * s, 26 * s); }
                bool future = pn.utc > NowMin();
                DT(g, L"\u25c6", F(FMS, 11 * s), x0, b, th.accent, AL, 0, th.shadow);
                std::wstring cd = PinCountdown(pn); FontE* fc = F(FMS, 12.5f * s); float cwid = TW(g, cd, fc);
                DT(g, cd, fc, x1, b, future ? th.accent : th.dim, AR, 0, th.shadow);
                CTime lt = ClockAt(g_localClock, (ULONGLONG)pn.utc * TPM);
                std::wstring when = Lower(kDays[lt.st.wDayOfWeek]) + L" " + Lower(FmtHMFull(lt.st)) + L" your time";
                FontE* fw = F(FM, 12 * s); float ww = TW(g, when, fw);
                float labRoom = x1 - x0 - 18 * s - cwid - 14 * s;
                if (labRoom - ww > 90 * s) { DT(g, when, fw, x1 - cwid - 14 * s, b, th.dim, AR, 0, th.shadow); labRoom -= ww + 10 * s; }
                DT(g, pn.label, F(FS, 13.5f * s), x0 + 18 * s, b, th.fg, AL, labRoom, th.shadow);
            }
        }

        // vim-style status bar
        { G::SolidBrush sb{ G::Color(th.statusBg) }; g.FillRectangle(&sb, bx0, sy, bx1 - bx0, kStatusH * s); }
        const wchar_t* mode = !g_flash.empty() ? L"ALARM" : (g_conv.on && g_shift) ? L"CONVERT" : g_shift ? L"SHIFT" : g_focus.phase == 1 ? L"FOCUS" : g_focus.phase ? L"BREAK" : L"NORMAL";
        FontE* fsb = F(FMS, 12 * s); float base = sy + 16.5f * s;
        DT(g, mode, fsb, bx0 + 12 * s, base, th.mode);
        float mw = TW(g, mode, fsb);
        SYSTEMTIME ut; FILETIME uf = UFt(fr.utc); FileTimeToSystemTime(&uf, &ut);
        std::wstring right = Fmt(L"utc %02d:%02d", ut.wHour, ut.wMinute);
        DT(g, right, fsb, bx1 - 12 * s, base, th.mode, AR);
        float rw2 = TW(g, right, fsb);
        std::wstring mid; int ai = NextAlarmIdx();
        RECT statusRc = { (LONG)bx0, (LONG)sy, (LONG)bx1, (LONG)(sy + kStatusH * s) };
        if (!g_flash.empty()) { mid = g_flash + L" \u00b7 click to dismiss"; v.flashRc = statusRc; }
        else if (g_conv.on && g_shift) { mid = g_conv.desc + L" \u00b7 click to copy"; v.flashRc = statusRc; }
        else if (g_shift) {
            mid = L"shifted " + FmtShift();
            for (size_t i = 0; i < fr.t.size(); i++) {   // say it out loud when a city lands on another day
                long long dd = DayNum(fr.t[i].st) - DayNum(fr.local.st);
                if (dd) { mid += L" \u00b7 " + DisplayName((int)i) + (dd > 0 ? L" is tomorrow" : L" is yesterday"); break; }
            }
            mid += L" \u00b7 middle-click: now";
        }
        else {
            mid = CalStatusText(120);
            if (mid.empty()) mid = DstShort();
            if (mid.empty() && ai >= 0) mid = AlarmFooterText(ai);
            if (mid.empty()) mid = CalStatusText(12 * 60);
            if (mid.empty() && !g_updTag.empty()) mid = L"update " + g_updTag + L" available \u00b7 right-click to install";
            if (mid.empty()) mid = Fmt(L"~/winclock \u00b7 %d clock%ls", (int)g_clocks.size(), g_clocks.size() == 1 ? L"" : L"s") +
                                   (S.profile != L"Default" ? L" \u00b7 " + Lower(S.profile) : L"");
        }
        float mx0 = bx0 + 12 * s + mw + 14 * s, mx1 = bx1 - 12 * s - rw2 - 14 * s;
        if (mx1 > mx0 + 20 * s) DT(g, mid, F(FS, 12.5f * s), (mx0 + mx1) / 2, base, th.statusFg, AC, mx1 - mx0);
    }
    v.W = w; v.H = h;
    Present(v.hwnd, v.surf, v.fly ? v.animAlpha : (BYTE)(S.opacity * 255 / 100), nullptr);
    if (!v.fly) KeepOnScreen(v.hwnd);
    long long secs = (long long)(fr.utc / 10000000ULL);
    v.key = (LayoutHasSeconds(layout) || TimerActive()) ? secs : secs / 60;
}

// ---------------------------------------------------------------- taskbar strip
static bool TaskbarIsLight() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS) return v != 0;
    return false;
}

static bool ForegroundIsFullscreen(HWND tray) {
    QUERY_USER_NOTIFICATION_STATE q;
    if (SUCCEEDED(SHQueryUserNotificationState(&q)) && (q == QUNS_RUNNING_D3D_FULL_SCREEN || q == QUNS_PRESENTATION_MODE)) return true;
    // When a real full-screen app is in front, Windows drops the taskbar out of the topmost band.
    // While the taskbar is still topmost, nothing is covering it, whatever size the front window is.
    if (tray && (GetWindowLongW(tray, GWL_EXSTYLE) & WS_EX_TOPMOST)) return false;
    HWND fg = GetForegroundWindow(); if (!fg || fg == tray) return false;
    LONG ex = GetWindowLongW(fg, GWL_EXSTYLE);
    if (ex & (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE)) return false;   // click-through overlays (GPU/monitor tools)
    wchar_t cls[64] = L""; GetClassNameW(fg, cls, 64);
    if (!wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Progman") || !wcscmp(cls, L"Shell_TrayWnd")) return false;
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid); if (pid == GetCurrentProcessId()) return false;
    RECT wr; if (!GetWindowRect(fg, &wr)) return false;
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromWindow(tray, MONITOR_DEFAULTTOPRIMARY), &mi);
    return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
}

static Bar* g_lastBar = &g_bar;
static std::vector<Bar*> g_extraBars;     // clocks on secondary-monitor taskbars
static void BarRaise() {
    auto raise = [](Bar& b) { if (b.hwnd && b.visible) SetWindowPos(b.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER); };
    raise(g_bar); for (auto b : g_extraBars) raise(*b);
}
static void HidePeek();
static int SafeColor(int st) { return st == ST_WORK ? 0 : st == ST_FREE ? 1 : 2; }

static void UpdateBarFor(Bar& b, HWND tray, bool primary, bool force) {
    if (!b.hwnd) return;
    b.items.clear();
    for (int i = 0; i < (int)g_clocks.size(); i++) if (S.tbWhich == 0 || g_clocks[i].taskbar) b.items.push_back(i);
    RECT twr{};
    bool show = S.tbShow && !b.items.empty() && tray && GetWindowRect(tray, &twr) && IsWindowVisible(tray);
    if (show) {
        MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromWindow(tray, MONITOR_DEFAULTTOPRIMARY), &mi);
        RECT vis; IntersectRect(&vis, &twr, &mi.rcMonitor);
        int full = std::min(twr.right - twr.left, twr.bottom - twr.top), seen = std::min(vis.right - vis.left, vis.bottom - vis.top);
        if (seen < full / 2) show = false;                  // auto-hidden taskbar
        if (show && ForegroundIsFullscreen(tray)) show = false;
    }
    if (!show) { if (b.visible) { ShowWindow(b.hwnd, SW_HIDE); b.visible = false; if (g_lastBar == &b) HidePeek(); } return; }

    int tw0 = twr.right - twr.left, th0 = twr.bottom - twr.top;
    b.horiz = tw0 >= th0;
    float d = (pGetDpiForWindow ? (float)pGetDpiForWindow(tray) : DpiOf(nullptr)) / 96.f; if (d <= 0) d = 1;
    RECT nr{}; HWND notify = primary ? FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr) : nullptr;
    bool nrOk = notify && GetWindowRect(notify, &nr) && nr.right > nr.left && nr.bottom > nr.top;

    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    long long key = (long long)((FtU(ft) / TPM)) + g_shift * 7 + b.rot * 1000003LL + b.hover * 31 + (S.h24 ? 5 : 0) + (long long)b.items.size() * 7919;
    std::wstring tmText = primary ? TimerBarText() : std::wstring();
    if (!tmText.empty()) key = key * 31 + (long long)std::hash<std::wstring>()(tmText);
    bool light = TaskbarIsLight();
    bool geomSame = EqualRect(&twr, &b.lastTb) && EqualRect(&nr, &b.lastTray);
    if (!force && key == b.key && geomSame && light == b.light && !b.dragging) { if (!b.visible) { ShowWindow(b.hwnd, SW_SHOWNOACTIVATE); b.visible = true; } return; }
    b.key = key; b.lastTb = twr; b.lastTray = nr; b.light = light;

    Frame fr = MakeFrame();
    FontsGC();
    int thick = b.horiz ? th0 : tw0;
    g_optScale = d;
    bool twoLine = S.tbLabels && (b.horiz ? thick >= 34 * d : true);
    FontE* fT = F(FS, 12.5f * d); FontE* fL = F(FR, 11.5f * d);

    auto labelOf = [&](int ci) {
        std::wstring l = DisplayName(ci);
        long long dd = DayNum(fr.t[ci].st) - DayNum(fr.local.st);
        if (dd) l += dd > 0 ? L" +1d" : L" −1d";
        return l;
    };
    // measure every clock once
    std::vector<float> widthAll;
    float maxW = 0;
    {
        HDC sdc = GetDC(nullptr); G::Graphics mg(sdc);
        float tw = TW(mg, S.h24 ? L"00:00" : L"00:00 PM", fT);
        for (int ci : b.items) {
            float w = tw;
            if (twoLine) w = std::max(w, TW(mg, labelOf(ci), fL));
            else if (S.tbLabels) w = TW(mg, labelOf(ci) + L"  ", fL) + tw;
            w += 18 * d; widthAll.push_back(w); maxW = std::max(maxW, w);
        }
        ReleaseDC(nullptr, sdc);
    }
    // too many to fit side by side (more than ~40% of the taskbar)? rotate through them instead
    float total = 0; for (float w : widthAll) total += w;
    float room = b.horiz ? tw0 * 0.4f : th0 * 0.5f;
    b.autoRotate = !S.tbRotate && b.items.size() > 1 && (b.horiz ? total : b.items.size() * 44 * d) > room;
    bool rotate = (S.tbRotate || b.autoRotate) && b.items.size() > 1;
    std::vector<int> shown = b.items; std::vector<float> widths = widthAll;
    if (rotate) {
        int per = 1;
        if (b.autoRotate && b.horiz) per = std::max(1, (int)(room / std::max(1.f, maxW)));
        per = std::min(per, (int)b.items.size());
        shown.clear();
        for (int k = 0; k < per; k++) shown.push_back(b.items[(b.rot * per + k) % b.items.size()]);
        widths.assign(shown.size(), maxW);
    }
    int W, H; b.cells.clear(); b.shown = shown; b.timerRc = RECT{};
    int tmW = 0;
    if (!tmText.empty()) { HDC sdc = GetDC(nullptr); { G::Graphics mg(sdc); tmW = (int)ceilf(TW(mg, tmText, fT) + 22 * d); } ReleaseDC(nullptr, sdc); }
    if (b.horiz) {
        H = thick; W = tmW;
        if (tmW) b.timerRc = { 0, 0, tmW, H };
        for (float w : widths) { b.cells.push_back({ W, 0, W + (int)ceilf(w), H }); W += (int)ceilf(w); }
    } else {
        W = thick; H = 0; int ch = (int)(44 * d);
        if (tmW) { b.timerRc = { 0, 0, W, ch }; H = ch; }
        for (size_t k = 0; k < widths.size(); k++) { b.cells.push_back({ 0, H, W, H + ch }); H += ch; }
    }
    W = std::max(W, 1); H = std::max(H, 1);
    int x, y;
    if (b.horiz) {
        int right = (S.tbAnchor == 2 && S.tbOffset >= 0) ? twr.right - (int)(S.tbOffset * d)
                  : (nrOk && nr.left > twr.left + 200 * d ? nr.left - (int)(6 * d) : twr.right - (int)((primary ? 300 : 170) * d));
        x = S.tbAnchor == 1 ? (int)twr.left + (int)(6 * d) : right - W;
        x = std::clamp(x, (int)twr.left, (int)twr.right - W); y = twr.top;
        if (b.dragging) x = b.lastX;
    } else {
        int bottom = (S.tbAnchor == 2 && S.tbOffset >= 0) ? twr.bottom - (int)(S.tbOffset * d)
                   : (nrOk && nr.top > twr.top + 200 * d ? nr.top - (int)(6 * d) : twr.bottom - (int)((primary ? 300 : 170) * d));
        y = S.tbAnchor == 1 ? (int)twr.top + (int)(6 * d) : bottom - H;
        y = std::clamp(y, (int)twr.top, (int)twr.bottom - H); x = twr.left;
        if (b.dragging) y = b.lastY;
    }
    b.lastX = x; b.lastY = y;

    EnsureSurface(b.surf, W, H);
    {
        G::Bitmap bmp(W, H, W * 4, PixelFormat32bppPARGB, (BYTE*)b.surf.bits);
        G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0x01000000));   // alpha 1: invisible but clickable
        DWORD fg = light ? NAVY : SNOW, dim = light ? 0xFF4A537F : ICE;
        if (!tmText.empty()) {   // focus timer / stopwatch, ticking every second
            RECT rc = b.timerRc; float cx = (rc.left + rc.right) / 2.f, cy = (rc.top + rc.bottom) / 2.f;
            FillRR(g, rc.left + 3 * d, rc.top + 7 * d, rc.right - rc.left - 6 * d, rc.bottom - rc.top - 14 * d, 6 * d, g_focus.phase == 1 ? A(PINK, 0x40) : A(ELECTRIC, 0x40));
            DT(g, tmText, fT, cx, cy + 4.5f * d, fg, AC);
        }
        for (size_t k = 0; k < shown.size(); k++) {
            int ci = shown[k]; RECT rc = b.cells[k]; const CTime& t = fr.t[ci];
            float cx = (rc.left + rc.right) / 2.f, cy = (rc.top + rc.bottom) / 2.f;
            if (b.hover == (int)k) FillRR(g, rc.left + 2 * d, rc.top + 4 * d, rc.right - rc.left - 4 * d, rc.bottom - rc.top - 8 * d, 5 * d,
                                         light ? A(NAVY, 0x14) : A(SNOW, 0x16));
            // the pip is the "safe to ping?" dot: pink = working now, periwinkle = free, grey = asleep / off
            int sc = SafeColor(ClockStatusAt(g_clocks[ci], t.st));
            G::SolidBrush pip{ G::Color(sc == 0 ? PINK : sc == 1 ? ELECTRIC_HI : A(dim, 0x90)) };
            if (twoLine) {
                DT(g, labelOf(ci), fL, cx, cy - 4 * d, dim, AC);
                DT(g, FmtHMFull(t.st), fT, cx, cy + 11.5f * d, fg, AC);
                g.FillEllipse(&pip, cx - 2 * d, rc.bottom - 6 * d, 4 * d, 4 * d);
            } else {
                std::wstring l = S.tbLabels ? labelOf(ci) + L"  " : L"";
                float lw = TW(g, l, fL), tw2 = TW(g, FmtHMFull(t.st), fT), x0 = cx - (lw + tw2) / 2;
                DT(g, l, fL, x0, cy + 4.5f * d, dim, AL);
                DT(g, FmtHMFull(t.st), fT, x0 + lw, cy + 4.5f * d, fg, AL);
                g.FillEllipse(&pip, cx - 2 * d, rc.bottom - 5 * d, 4 * d, 4 * d);
            }
        }
    }
    b.W = W; b.H = H;
    POINT pos{ x, y }; Present(b.hwnd, b.surf, 255, &pos);
    if (!b.visible) { ShowWindow(b.hwnd, SW_SHOWNOACTIVATE); b.visible = true; }
}

static LRESULT CALLBACK BarProc(HWND h, UINT m, WPARAM wp, LPARAM lp);
static HWND CreateBarWindow(Bar* b) {
    HWND h = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"WinClockBar", L"WinClock Taskbar",
                             WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)b);
    b->hwnd = h;
    return h;
}
static void UpdateBar(bool force) {
    UpdateBarFor(g_bar, FindWindowW(L"Shell_TrayWnd", nullptr), true, force);
    // secondary taskbars (one per extra monitor)
    std::vector<HWND> trays;
    if (S.tbAllMon) for (HWND t = nullptr; (t = FindWindowExW(nullptr, t, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr && trays.size() < 8;) trays.push_back(t);
    while (g_extraBars.size() > trays.size()) {
        Bar* b = g_extraBars.back(); g_extraBars.pop_back();
        if (g_lastBar == b) { HidePeek(); g_lastBar = &g_bar; }
        DestroyWindow(b->hwnd); FreeSurface(b->surf); delete b;
    }
    while (g_extraBars.size() < trays.size()) { Bar* b = new Bar(); CreateBarWindow(b); g_extraBars.push_back(b); }
    for (size_t k = 0; k < trays.size(); k++) UpdateBarFor(*g_extraBars[k], trays[k], false, force);
    BarRaise();
}

// ---------------------------------------------------------------- tray icon
static void UpdateTrayTip() {
    if (!g_nid.hWnd || !g_trayAdded) return;
    Frame fr = MakeFrame(); std::wstring s;
    for (size_t i = 0; i < g_clocks.size(); i++) {
        std::wstring line = g_clocks[i].label + L"  " + FmtHMFull(fr.t[i].st) + L"\n";
        if (s.size() + line.size() > 120) break;
        s += line;
    }
    if (s.empty()) s = L"WinClock"; else s.pop_back();
    wcsncpy(g_nid.szTip, s.c_str(), 127); g_nid.szTip[127] = 0;
    g_nid.uFlags = NIF_TIP | NIF_SHOWTIP; Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void RemoveTrayIcon() { if (g_trayAdded) { Shell_NotifyIconW(NIM_DELETE, &g_nid); g_trayAdded = false; } }
static void AddTrayIcon() {
    RemoveTrayIcon();
    g_nid = NOTIFYICONDATAW{}; g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_main; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP; g_nid.uCallbackMessage = WM_APP_TRAY;
    LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &g_nid.hIcon);
    wcscpy(g_nid.szTip, L"WinClock");
    g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
    g_nid.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    UpdateTrayTip();
}

#include "extras.inc"

// ---------------------------------------------------------------- scheduling
static bool NeedsSeconds() {
    if (TimerActive()) return true;
    if (g_widget.hwnd && S.visible && LayoutHasSeconds(S.layout)) return true;
    if (g_flyout.hwnd && IsWindowVisible(g_flyout.hwnd) && LayoutHasSeconds(S.flyLayout)) return true;
    return false;
}
static void ScheduleTick() {
    if (g_paused) { KillTimer(g_main, TM_TICK); return; }
    SYSTEMTIME st; GetSystemTime(&st);
    UINT ms = NeedsSeconds() ? 1000 - st.wMilliseconds + 8 : (60 - st.wSecond) * 1000 - st.wMilliseconds + 8;
    SetTimer(g_main, TM_TICK, ms, nullptr);
}
static void ScheduleBarTimer() {
    if (g_paused || (!S.tbShow && S.zmode != Z_TOP)) { KillTimer(g_main, TM_BAR); return; }
    APPBARDATA abd{}; abd.cbSize = sizeof(abd);
    bool autohide = (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) != 0;
    SetTimer(g_main, TM_BAR, autohide ? 250 : 2000, nullptr);
}
static void Tick(bool force) {
    if (g_paused) return;
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    long long secs = (long long)((FtU(ft) + (long long)g_shift * TPM) / 10000000ULL);
    auto check = [&](View& v, int layout) {
        if (!v.hwnd || !IsWindowVisible(v.hwnd)) return;
        long long k = (LayoutHasSeconds(layout) || TimerActive()) ? secs : secs / 60;
        if (force || k != v.key) PaintView(v);
    };
    if (TimerActive()) FocusTick();
    check(g_widget, S.layout); check(g_flyout, S.flyLayout);
    UpdateBar(force);
    static long long lastTipMin = -1;
    if (force || secs / 60 != lastTipMin) { lastTipMin = secs / 60; UpdateTrayTip(); }
    UpdateTrayIconImage(false);
}
static void RefreshAll() { Tick(true); ScheduleTick(); ScheduleBarTimer(); }

static long long g_lastChimeMin = -1;
static void ScheduleAlarmTimer() {
    SYSTEMTIME st; GetSystemTime(&st);
    SetTimer(g_main, TM_ALARM, (60 - st.wSecond) * 1000 - st.wMilliseconds + 60, nullptr);
}
static void AlarmTick() {
    long long now = NowMin();
    bool hadFlash = !g_flash.empty();
    CheckAlarms();
    if (!g_flash.empty() && GetTickCount64() > g_flashUntil) g_flash.clear();
    if (S.chime && !g_paused && now != g_lastChimeMin) {
        CTime t = ClockAt(g_localClock, (ULONGLONG)now * TPM);
        if (t.st.wMinute == 0) { g_lastChimeMin = now; MessageBeep(MB_OK); }
    }
    bool repaint = hadFlash != !g_flash.empty() || !g_flash.empty();
    if (PrunePins()) repaint = true;
    if (now % 60 == 0 || g_dstAt < 0) CheckDstNotify();
    CalTick(); UpdateTick(); WeatherTick();
    if (AutoProfileTick()) repaint = true;
    if (repaint) RefreshAll();
    ScheduleAlarmTimer();
}

// ---------------------------------------------------------------- widget placement / style
// "Pin to desktop": sit just above the desktop-icons layer, not at the very bottom. With live wallpapers
// (Wallpaper Engine and similar) the icons layer is a full-screen window of its own; below it the widget
// would still show but every click would land on the desktop instead.
static HWND DesktopIconsHost() {
    HWND found = nullptr;
    EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        if (FindWindowExW(w, nullptr, L"SHELLDLL_DefView", nullptr)) { *(HWND*)lp = w; return FALSE; }
        return TRUE;
    }, (LPARAM)&found);
    return found;
}
static HWND DesktopInsertAfter(HWND self) {
    HWND host = DesktopIconsHost();
    if (!host) return HWND_BOTTOM;
    HWND prev = GetWindow(host, GW_HWNDPREV);
    while (prev == self) prev = GetWindow(prev, GW_HWNDPREV);
    return prev ? prev : HWND_TOP;
}
static void PinToDesktop(HWND w) {
    if (!w) return;
    HWND host = DesktopIconsHost();
    if (host && GetWindow(host, GW_HWNDPREV) == w) return;   // already right above the icons
    SetWindowPos(w, DesktopInsertAfter(w), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
static void ApplyWidgetStyle() {
    HWND w = g_widget.hwnd; if (!w) return;
    LONG_PTR ex = GetWindowLongPtrW(w, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    if (S.clickThrough) ex |= WS_EX_TRANSPARENT; else ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(w, GWL_EXSTYLE, ex);
    g_deskRaised = false;
    if (S.zmode == Z_TOP) SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    else {
        SetWindowPos(w, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        PinToDesktop(w);
    }
}
static void PlaceWidget() {
    HWND w = g_widget.hwnd;
    RECT r{ S.x, S.y, S.x + g_widget.W, S.y + g_widget.H };
    if (S.x == INT_MIN || !MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) {
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        S.x = wa.right - g_widget.W - 28; S.y = wa.top + 28;
    }
    SetWindowPos(w, nullptr, S.x, S.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void ShowWidget(bool show) {
    S.visible = show;
    if (show) { PaintView(g_widget); ShowWindow(g_widget.hwnd, SW_SHOWNOACTIVATE); ApplyWidgetStyle(); }
    else { ShowWindow(g_widget.hwnd, SW_HIDE); TipHide(g_widget.tip); }
    SaveSettings(); ScheduleTick();
}

static bool g_fsHidden = false;
static void CheckWidgetFullscreen() {
    if (!g_widget.hwnd) return;
    if (!S.visible || S.zmode != Z_TOP) {
        if (g_fsHidden) { g_fsHidden = false; if (S.visible) { ShowWindow(g_widget.hwnd, SW_SHOWNOACTIVATE); PaintView(g_widget); } }
        return;
    }
    bool fs = ForegroundIsFullscreen(FindWindowW(L"Shell_TrayWnd", nullptr));
    if (fs && !g_fsHidden) { ShowWindow(g_widget.hwnd, SW_HIDE); TipHide(g_widget.tip); g_fsHidden = true; }
    else if (!fs && g_fsHidden) { g_fsHidden = false; ShowWindow(g_widget.hwnd, SW_SHOWNOACTIVATE); PaintView(g_widget); }
}

// ---------------------------------------------------------------- startup (Run key)
static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static bool StartupEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, L"WinClock", RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}
static void SetStartup(bool on) {
    HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH);
        std::wstring v = L"\"" + std::wstring(p) + L"\"";
        RegSetValueExW(k, L"WinClock", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"WinClock");
    RegCloseKey(k);
}

// ---------------------------------------------------------------- add / edit clock dialog
struct ZoneEntry { std::wstring text, tzid, city; long bias; std::vector<std::pair<std::wstring, std::wstring>> group; };
struct DlgState {
    bool edit = false; std::wstring label, tzid; bool taskbar = false;
    std::vector<std::pair<std::wstring, std::wstring>> picks;   // (label, tzid) to add
    std::vector<ZoneEntry> zones; bool labelEdited = false, settingLabel = false;
};

static std::wstring ZoneDisplay(const wchar_t* key) {
    std::wstring path = std::wstring(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\") + key;
    HKEY k; std::wstring out;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &k) == ERROR_SUCCESS) {
        wchar_t buf[256]; DWORD need = 0;
        if (RegLoadMUIStringW(k, L"MUI_Display", buf, sizeof(buf), &need, 0, nullptr) == ERROR_SUCCESS) out = buf;
        else { DWORD sz = sizeof(buf); if (RegQueryValueExW(k, L"Display", nullptr, nullptr, (BYTE*)buf, &sz) == ERROR_SUCCESS) out = buf; }
        RegCloseKey(k);
    }
    return out.empty() ? key : out;
}

static std::vector<ZoneEntry> BuildZoneEntries() {
    std::vector<ZoneEntry> zones, out;
    for (DWORD i = 0;; ++i) {
        DYNAMIC_TIME_ZONE_INFORMATION d{};
        if (EnumDynamicTimeZoneInformation(i, &d) != ERROR_SUCCESS) break;
        ZoneEntry e; e.tzid = d.TimeZoneKeyName; e.text = ZoneDisplay(d.TimeZoneKeyName); e.bias = d.Bias;
        size_t p = e.text.find(L") "); std::wstring rest = p == std::wstring::npos ? e.text : e.text.substr(p + 2);
        e.city = rest.substr(0, rest.find(L','));
        zones.push_back(e);
    }
    std::sort(zones.begin(), zones.end(), [](const ZoneEntry& a, const ZoneEntry& b) { return a.bias != b.bias ? a.bias > b.bias : a.text < b.text; });
    out.push_back({ L"⌂  Local time (this PC)", L"Local", L"Local", 0 });
    out.push_back({ L"UTC — Coordinated Universal Time", L"UTC", L"UTC", 0 });
    for (auto& a : kAliases) {
        for (auto& z : zones) if (_wcsicmp(z.tzid.c_str(), a.tz) == 0) {
            std::wstring city = a.city; std::wstring shortCity = city.substr(0, city.find(L','));
            out.push_back({ city + L"   —   " + z.text, z.tzid, shortCity, z.bias }); break;
        }
    }
    // whole countries: one entry adds every zone in the country
    struct Country { const wchar_t* name; const wchar_t* members; };
    static const Country kCountries[] = {
        { L"United States", L"New York=Eastern Standard Time;Chicago=Central Standard Time;Denver=Mountain Standard Time;Phoenix=US Mountain Standard Time;Los Angeles=Pacific Standard Time;Anchorage=Alaskan Standard Time;Honolulu=Hawaiian Standard Time" },
        { L"Canada", L"St. John's=Newfoundland Standard Time;Halifax=Atlantic Standard Time;Toronto=Eastern Standard Time;Winnipeg=Central Standard Time;Regina=Canada Central Standard Time;Edmonton=Mountain Standard Time;Vancouver=Pacific Standard Time" },
        { L"Australia", L"Perth=W. Australia Standard Time;Darwin=AUS Central Standard Time;Adelaide=Cen. Australia Standard Time;Brisbane=E. Australia Standard Time;Sydney=AUS Eastern Standard Time;Hobart=Tasmania Standard Time" },
        { L"Russia", L"Kaliningrad=Kaliningrad Standard Time;Moscow=Russian Standard Time;Samara=Russia Time Zone 3;Yekaterinburg=Ekaterinburg Standard Time;Omsk=Omsk Standard Time;Novosibirsk=N. Central Asia Standard Time;Krasnoyarsk=North Asia Standard Time;Irkutsk=North Asia East Standard Time;Yakutsk=Yakutsk Standard Time;Vladivostok=Vladivostok Standard Time;Magadan=Magadan Standard Time;Kamchatka=Russia Time Zone 11" },
        { L"Brazil", L"S\u00e3o Paulo=E. South America Standard Time;Fortaleza=SA Eastern Standard Time;Cuiab\u00e1=Central Brazilian Standard Time;Manaus=SA Western Standard Time;Rio Branco=SA Pacific Standard Time" },
        { L"Mexico", L"Mexico City=Central Standard Time (Mexico);Canc\u00fan=Eastern Standard Time (Mexico);Chihuahua=Mountain Standard Time (Mexico);Tijuana=Pacific Standard Time (Mexico)" },
        { L"Indonesia", L"Jakarta=SE Asia Standard Time;Makassar=Singapore Standard Time;Jayapura=Tokyo Standard Time" },
        { L"New Zealand", L"Auckland=New Zealand Standard Time;Chatham Islands=Chatham Islands Standard Time" },
        { L"Chile", L"Santiago=Pacific SA Standard Time;Easter Island=Easter Island Standard Time" },
        { L"Portugal", L"Lisbon=GMT Standard Time;Azores=Azores Standard Time" },
        { L"Spain", L"Madrid=Romance Standard Time;Canary Islands=GMT Standard Time" },
        { L"Europe (west / central / east)", L"London=GMT Standard Time;Berlin=W. Europe Standard Time;Athens=GTB Standard Time" },
        { L"Asia business hubs", L"Dubai=Arabian Standard Time;Mumbai=India Standard Time;Singapore=Singapore Standard Time;Hong Kong=China Standard Time;Tokyo=Tokyo Standard Time" },
    };
    std::vector<ZoneEntry> groups;
    for (auto& c : kCountries) {
        ZoneEntry e; e.city = c.name; e.tzid = L"group"; e.bias = 0;
        std::wstring m = c.members;
        for (size_t p = 0; p < m.size();) {
            size_t q = m.find(L';', p); if (q == std::wstring::npos) q = m.size();
            std::wstring it = m.substr(p, q - p); size_t eq = it.find(L'=');
            if (eq != std::wstring::npos) {
                std::wstring tz = it.substr(eq + 1);
                for (auto& z : zones) if (_wcsicmp(z.tzid.c_str(), tz.c_str()) == 0) { e.group.push_back({ it.substr(0, eq), tz }); break; }
            }
            p = q + 1;
        }
        if (e.group.size() < 2) continue;
        e.text = Fmt(L">  %ls \u2014 all %d time zones  (", c.name, (int)e.group.size());
        for (size_t k = 0; k < e.group.size(); k++) e.text += (k ? L", " : L"") + e.group[k].first;
        e.text += L")";
        groups.push_back(e);
    }
    out.insert(out.begin() + 2, groups.begin(), groups.end());
    for (auto& z : zones) out.push_back(z);
    return out;
}

static void DlgFilter(HWND d, DlgState* st) {
    wchar_t q[128]; GetDlgItemTextW(d, IDC_SEARCH, q, 128);
    std::wstring ql = Lower(q); std::vector<std::wstring> words;
    for (size_t p = 0; p < ql.size();) { size_t e = ql.find(L' ', p); if (e == std::wstring::npos) e = ql.size(); if (e > p) words.push_back(ql.substr(p, e - p)); p = e + 1; }
    HWND lb = GetDlgItem(d, IDC_LIST);
    SendMessageW(lb, WM_SETREDRAW, FALSE, 0); SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < st->zones.size(); i++) {
        std::wstring t = Lower(st->zones[i].text + L" " + st->zones[i].tzid);
        bool ok = true; for (auto& w : words) if (t.find(w) == std::wstring::npos) { ok = false; break; }
        if (!ok) continue;
        int idx = (int)SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)st->zones[i].text.c_str());
        SendMessageW(lb, LB_SETITEMDATA, idx, (LPARAM)i);
    }
    SendMessageW(lb, WM_SETREDRAW, TRUE, 0); InvalidateRect(lb, nullptr, TRUE);
}
static std::vector<int> DlgSelected(HWND d) {
    HWND lb = GetDlgItem(d, IDC_LIST); int n = (int)SendMessageW(lb, LB_GETSELCOUNT, 0, 0);
    std::vector<int> items(std::max(n, 0));
    if (n > 0) SendMessageW(lb, LB_GETSELITEMS, n, (LPARAM)items.data());
    return items;
}
static void DlgOnSel(HWND d, DlgState* st) {
    HWND lb = GetDlgItem(d, IDC_LIST);
    std::vector<int> selItems = DlgSelected(d);
    if (selItems.empty()) { SetDlgItemTextW(d, IDC_PREVIEW, L""); EnableWindow(GetDlgItem(d, IDC_LABEL), TRUE); return; }
    size_t total = 0; for (int it : selItems) { const ZoneEntry& z = st->zones[(size_t)SendMessageW(lb, LB_GETITEMDATA, it, 0)]; total += z.group.empty() ? 1 : z.group.size(); }
    bool multi = total > 1 && !st->edit;
    EnableWindow(GetDlgItem(d, IDC_LABEL), !multi);
    if (multi) { SetDlgItemTextW(d, IDC_PREVIEW, Fmt(L"%d clocks will be added, each labelled with its city.  (Ctrl/Shift-click to pick more)", (int)total).c_str()); return; }
    int sel = selItems[0];
    const ZoneEntry& e = st->zones[(size_t)SendMessageW(lb, LB_GETITEMDATA, sel, 0)];
    if (!e.group.empty()) { SetDlgItemTextW(d, IDC_PREVIEW, L"Pick one zone from this country to edit a clock."); return; }
    if (!st->labelEdited) { st->settingLabel = true; SetDlgItemTextW(d, IDC_LABEL, e.city.c_str()); st->settingLabel = false; }
    Clock c; c.tzid = e.tzid; ResolveClock(c);
    FILETIME ft; GetSystemTimeAsFileTime(&ft); CTime t = ClockAt(c, FtU(ft));
    SetDlgItemTextW(d, IDC_PREVIEW, Fmt(L"Time there now:  %ls, %d %ls  %ls   (%ls%ls)", kDays[t.st.wDayOfWeek], t.st.wDay, kMonths[t.st.wMonth - 1],
                                        FmtHMFull(t.st).c_str(), FmtUtc(t.offset).c_str(), t.dst ? L", DST" : L"").c_str());
}
static LRESULT CALLBACK SearchSubclass(HWND h, UINT m, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    if (m == WM_KEYDOWN && (wp == VK_DOWN || wp == VK_UP || wp == VK_NEXT || wp == VK_PRIOR)) {
        SendMessageW((HWND)ref, WM_KEYDOWN, wp, lp); return 0;
    }
    return DefSubclassProc(h, m, wp, lp);
}
static INT_PTR CALLBACK ClockDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    DlgState* st = (DlgState*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG: {
        st = (DlgState*)lp; SetWindowLongPtrW(d, DWLP_USER, lp);
        HICON ic = nullptr; LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &ic); SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)ic);
        st->zones = BuildZoneEntries();
        SetWindowTextW(d, st->edit ? L"Edit clock" : L"Add clocks");
        st->settingLabel = true; SetDlgItemTextW(d, IDC_LABEL, st->label.c_str()); st->settingLabel = false;
        st->labelEdited = st->edit;
        CheckDlgButton(d, IDC_TASKBAR, st->taskbar ? BST_CHECKED : BST_UNCHECKED);
        DlgFilter(d, st);
        HWND lb = GetDlgItem(d, IDC_LIST);
        if (st->edit) {
            int cnt = (int)SendMessageW(lb, LB_GETCOUNT, 0, 0), pick = -1;
            for (int i = 0; i < cnt; i++) {
                const ZoneEntry& e = st->zones[(size_t)SendMessageW(lb, LB_GETITEMDATA, i, 0)];
                if (_wcsicmp(e.tzid.c_str(), st->tzid.c_str()) == 0) { if (pick < 0) pick = i; if (e.city == st->label) { pick = i; break; } }
            }
            if (pick >= 0) { SendMessageW(lb, LB_SETSEL, TRUE, pick); SendMessageW(lb, LB_SETCARETINDEX, pick, 0); DlgOnSel(d, st); }
        }
        SetWindowSubclass(GetDlgItem(d, IDC_SEARCH), SearchSubclass, 1, (DWORD_PTR)lb);
        SetForegroundWindow(d);
        SetFocus(GetDlgItem(d, IDC_SEARCH));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SEARCH:
            if (HIWORD(wp) == EN_CHANGE) { DlgFilter(d, st); SendDlgItemMessageW(d, IDC_LIST, LB_SETSEL, TRUE, 0); SendDlgItemMessageW(d, IDC_LIST, LB_SETCARETINDEX, 0, 0); DlgOnSel(d, st); }
            break;
        case IDC_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE) DlgOnSel(d, st);
            else if (HIWORD(wp) == LBN_DBLCLK) PostMessageW(d, WM_COMMAND, IDOK, 0);
            break;
        case IDC_LABEL:
            if (HIWORD(wp) == EN_CHANGE && !st->settingLabel) st->labelEdited = true;
            break;
        case IDOK: {
            HWND lb = GetDlgItem(d, IDC_LIST);
            std::vector<int> selItems = DlgSelected(d);
            st->picks.clear();
            for (int it : selItems) {
                const ZoneEntry& z = st->zones[(size_t)SendMessageW(lb, LB_GETITEMDATA, it, 0)];
                if (z.group.empty()) st->picks.push_back({ z.city, z.tzid });
                else for (auto& gm : z.group) st->picks.push_back(gm);
            }
            if (st->picks.empty()) { if (!st->edit) { MessageBeep(MB_ICONWARNING); return TRUE; } st->picks.push_back({ st->label, st->tzid }); }
            wchar_t lab[128]; GetDlgItemTextW(d, IDC_LABEL, lab, 128);
            if (st->picks.size() == 1 && lab[0]) st->picks[0].first = lab;
            st->label = st->picks[0].first; st->tzid = st->picks[0].second;
            st->taskbar = IsDlgButtonChecked(d, IDC_TASKBAR) == BST_CHECKED;
            EndDialog(d, IDOK); return TRUE;
        }
        case IDCANCEL: EndDialog(d, IDCANCEL); return TRUE;
        }
        break;
    case WM_DESTROY: RemoveWindowSubclass(GetDlgItem(d, IDC_SEARCH), SearchSubclass, 1); break;
    }
    return FALSE;
}
static void ClockDialog(int idx) {   // idx < 0 = add
    static bool open = false; if (open) return; open = true;
    DlgState st; st.edit = idx >= 0;
    if (st.edit) { st.label = g_clocks[idx].label; st.tzid = g_clocks[idx].tzid; st.taskbar = g_clocks[idx].taskbar; }
    else st.taskbar = std::count_if(g_clocks.begin(), g_clocks.end(), [](const Clock& c) { return c.taskbar; }) < 2;
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_CLOCK), g_main, ClockDlgProc, (LPARAM)&st) == IDOK && g_clocks.size() < 50) {
        if (st.edit && idx < (int)g_clocks.size()) {
            Clock c; c.label = st.label; c.tzid = st.tzid; c.taskbar = st.taskbar; ResolveClock(c); g_clocks[idx] = c;
        } else {
            for (auto& pk : st.picks) {
                bool dup = false;
                for (auto& e : g_clocks) if (e.label == pk.first && _wcsicmp(e.tzid.c_str(), pk.second.c_str()) == 0) dup = true;
                if (dup || g_clocks.size() >= 50) continue;
                Clock c; c.label = pk.first; c.tzid = pk.second; c.taskbar = st.taskbar && st.picks.size() == 1 ? true : (st.taskbar && st.picks.size() <= 3); ResolveClock(c);
                g_clocks.push_back(c);
            }
        }
        SaveClocks(); RefreshAll();
    }
    open = false;
    TrimMemory();
}

// ---------------------------------------------------------------- parsing helpers
static bool ParseTime(std::wstring s, int& h, int& m) {
    s = Lower(s); std::wstring t; for (wchar_t c : s) if (c != L' ') t += c;
    int ap = 0;
    auto ends = [&](const wchar_t* suf) { size_t n = wcslen(suf); if (t.size() >= n && t.compare(t.size() - n, n, suf) == 0) { t.erase(t.size() - n); return true; } return false; };
    if (ends(L"am") || ends(L"a")) ap = 1; else if (ends(L"pm") || ends(L"p")) ap = 2;
    if (t.empty()) return false;
    size_t sep = t.find_first_of(L":.");
    std::wstring hs, ms;
    if (sep != std::wstring::npos) { hs = t.substr(0, sep); ms = t.substr(sep + 1); }
    else if (t.size() <= 2) { hs = t; ms = L"0"; }
    else { hs = t.substr(0, t.size() - 2); ms = t.substr(t.size() - 2); }
    if (hs.empty() || ms.empty() || hs.size() > 2 || ms.size() > 2) return false;
    for (wchar_t c : hs + ms) if (!iswdigit(c)) return false;
    h = _wtoi(hs.c_str()); m = _wtoi(ms.c_str());
    if (ap) { if (h < 1 || h > 12) return false; if (ap == 2 && h < 12) h += 12; if (ap == 1 && h == 12) h = 0; }
    return h >= 0 && h <= 23 && m >= 0 && m <= 59;
}
static bool ParseDate(const std::wstring& s, int& y, int& mo, int& d) {
    int a, b, c; wchar_t s1, s2;
    if (swscanf(s.c_str(), L" %d%lc%d%lc%d", &a, &s1, &b, &s2, &c) != 5) return false;
    if (a > 31) { y = a; mo = b; d = c; } else { d = a; mo = b; y = c; }
    if (y < 100) y += 2000;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || y < 1990 || y > 2200) return false;
    SYSTEMTIME st{}; st.wYear = (WORD)y; st.wMonth = (WORD)mo; st.wDay = (WORD)d; FILETIME f;
    return SystemTimeToFileTime(&st, &f) != 0;
}
typedef std::vector<std::pair<std::wstring, std::wstring>> ZoneOpts;
static ZoneOpts ZoneOptions() {
    ZoneOpts o; bool hasLocal = false, hasUtc = false;
    for (auto& c : g_clocks) { o.push_back({ c.label, c.tzid }); if (c.kind == 1) hasLocal = true; if (c.kind == 2) hasUtc = true; }
    if (!hasLocal) o.push_back({ L"Your local time", L"Local" });
    if (!hasUtc) o.push_back({ L"UTC", L"UTC" });
    return o;
}
static void SetDlgIcon(HWND d) { HICON ic = nullptr; LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &ic); SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)ic); }

// ---------------------------------------------------------------- alarm dialog
struct AlarmDlg { Alarm a; bool edit = false; ZoneOpts opts; };
static bool ReadAlarmFields(HWND d, AlarmDlg* st, Alarm& out, std::wstring& err) {
    wchar_t b[128]; out = st->a;
    GetDlgItemTextW(d, IDC_ALABEL, b, 128); out.label = b;
    while (!out.label.empty() && out.label.back() == L' ') out.label.pop_back();
    if (out.label.empty()) out.label = L"Alarm";
    GetDlgItemTextW(d, IDC_ATIME, b, 128);
    if (!ParseTime(b, out.hh, out.mm)) { err = L"Type a time like 9:30, 9:30 am or 17:45."; return false; }
    int ci = (int)SendDlgItemMessageW(d, IDC_ACLOCK, CB_GETCURSEL, 0, 0);
    if (ci >= 0 && ci < (int)st->opts.size()) out.tzid = st->opts[ci].second;
    out.rep = std::clamp((int)SendDlgItemMessageW(d, IDC_AREPEAT, CB_GETCURSEL, 0, 0), 0, 3);
    GetDlgItemTextW(d, IDC_ADATE, b, 128); std::wstring ds = b;
    out.y = out.mo = out.d = 0;
    if (ds.find_first_not_of(L' ') != std::wstring::npos) {
        if (!ParseDate(ds, out.y, out.mo, out.d)) { err = L"Type the date as YYYY-MM-DD, or leave it empty."; return false; }
        out.rep = 0;
    }
    out.enabled = IsDlgButtonChecked(d, IDC_AENABLED) == BST_CHECKED;
    int li = (int)SendDlgItemMessageW(d, IDC_ALEAD, CB_GETCURSEL, 0, 0);
    out.lead = kLeads[std::clamp(li, 0, (int)(sizeof(kLeads) / sizeof(*kLeads)) - 1)];
    return true;
}
static void AlarmDlgPreview(HWND d, AlarmDlg* st) {
    Alarm a; std::wstring err;
    if (!ReadAlarmFields(d, st, a, err)) { SetDlgItemTextW(d, IDC_APREVIEW, err.c_str()); return; }
    long long now = NowMin(); ComputeNext(a, now);
    if (a.next < 0) { SetDlgItemTextW(d, IDC_APREVIEW, L"That moment has already passed."); return; }
    CTime local = ClockAt(g_localClock, (ULONGLONG)a.next * TPM);
    std::wstring s = L"Next: " + AlarmWhen(a, true) + L"\n= " + kDays[local.st.wDayOfWeek] + L" " + FmtHMFull(local.st) +
                     L" your time  ·  in " + FmtSpan(std::max(1LL, a.next - now));
    if (a.y) s += L"  (one-time countdown)";
    if (a.lead) { CTime r = ClockAt(g_localClock, (ULONGLONG)(a.next - a.lead) * TPM); s += L"\nReminder at " + FmtHMFull(r.st) + L" your time"; }
    SetDlgItemTextW(d, IDC_APREVIEW, s.c_str());
}
static INT_PTR CALLBACK AlarmDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    AlarmDlg* st = (AlarmDlg*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG: {
        st = (AlarmDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        if (st->edit) SetWindowTextW(d, L"Edit alarm / countdown");
        SetDlgItemTextW(d, IDC_ALABEL, st->a.label.c_str());
        SYSTEMTIME t{}; t.wHour = (WORD)st->a.hh; t.wMinute = (WORD)st->a.mm; SetDlgItemTextW(d, IDC_ATIME, FmtHMFull(t).c_str());
        if (st->a.y) SetDlgItemTextW(d, IDC_ADATE, Fmt(L"%04d-%02d-%02d", st->a.y, st->a.mo, st->a.d).c_str());
        int sel = 0;
        for (size_t i = 0; i < st->opts.size(); i++) {
            SendDlgItemMessageW(d, IDC_ACLOCK, CB_ADDSTRING, 0, (LPARAM)st->opts[i].first.c_str());
            if (_wcsicmp(st->opts[i].second.c_str(), st->a.tzid.c_str()) == 0) sel = (int)i;
        }
        SendDlgItemMessageW(d, IDC_ACLOCK, CB_SETCURSEL, sel, 0);
        for (auto r : kRepeat) SendDlgItemMessageW(d, IDC_AREPEAT, CB_ADDSTRING, 0, (LPARAM)r);
        SendDlgItemMessageW(d, IDC_AREPEAT, CB_SETCURSEL, st->a.rep, 0);
        CheckDlgButton(d, IDC_AENABLED, st->a.enabled ? BST_CHECKED : BST_UNCHECKED);
        { int ls = 0;
          for (int i = 0; i < (int)(sizeof(kLeads) / sizeof(*kLeads)); i++) {
              int v = kLeads[i];
              std::wstring t = !v ? std::wstring(L"At the time") : v < 60 ? Fmt(L"%d min before (leave-by reminder)", v) : v < 1440 ? Fmt(L"%d h before (leave-by reminder)", v / 60) : std::wstring(L"1 day before");
              SendDlgItemMessageW(d, IDC_ALEAD, CB_ADDSTRING, 0, (LPARAM)t.c_str());
              if (v == st->a.lead) ls = i;
          }
          SendDlgItemMessageW(d, IDC_ALEAD, CB_SETCURSEL, ls, 0); }
        AlarmDlgPreview(d, st);
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDC_ALABEL)); SendDlgItemMessageW(d, IDC_ALABEL, EM_SETSEL, 0, -1);
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_ALABEL: case IDC_ATIME: case IDC_ADATE: if (HIWORD(wp) == EN_CHANGE) AlarmDlgPreview(d, st); break;
        case IDC_ACLOCK: case IDC_AREPEAT: case IDC_ALEAD: if (HIWORD(wp) == CBN_SELCHANGE) AlarmDlgPreview(d, st); break;
        case IDOK: {
            Alarm a; std::wstring err;
            if (!ReadAlarmFields(d, st, a, err)) { MessageBoxW(d, err.c_str(), L"WinClock", MB_OK | MB_ICONWARNING); return TRUE; }
            st->a = a; EndDialog(d, IDOK); return TRUE;
        }
        case IDCANCEL: EndDialog(d, IDCANCEL); return TRUE;
        }
        break;
    }
    return FALSE;
}
static void AlarmDialog(int idx, int lead = 0);
static void AlarmDialog(int idx, int lead) {
    static bool open = false; if (open) return; open = true;
    AlarmDlg st; st.opts = ZoneOptions(); st.edit = idx >= 0;
    if (st.edit) st.a = g_alarms[idx];
    else { st.a.label = lead ? L"Flight" : L"Meeting"; st.a.tzid = st.opts[0].second; SYSTEMTIME lt; GetLocalTime(&lt); st.a.hh = (lt.wHour + 1) % 24; st.a.mm = 0; st.a.lead = lead; }
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_ALARM), g_main, AlarmDlgProc, (LPARAM)&st) == IDOK) {
        st.a.next = -1; if (st.a.enabled) ComputeNext(st.a, NowMin());
        if (st.edit && idx < (int)g_alarms.size()) g_alarms[idx] = st.a; else if (g_alarms.size() < 50) g_alarms.push_back(st.a);
        SaveAlarms(); RefreshAll();
    }
    open = false; TrimMemory();
}

// ---------------------------------------------------------------- jump-to-time dialog
struct JumpDlg { ZoneOpts opts; long long target = 0; };
static bool JumpRead(HWND d, JumpDlg* st, long long& target) {
    wchar_t b[64]; GetDlgItemTextW(d, IDC_JTIME, b, 64); int hh, mm;
    if (!ParseTime(b, hh, mm)) return false;
    int ci = (int)SendDlgItemMessageW(d, IDC_JCLOCK, CB_GETCURSEL, 0, 0); if (ci < 0 || ci >= (int)st->opts.size()) return false;
    long long day = (long long)SendDlgItemMessageW(d, IDC_JDAY, CB_GETCURSEL, 0, 0) - 1;
    Clock c = ZoneClock(st->opts[ci].second);
    FILETIME f; GetSystemTimeAsFileTime(&f); CTime zt = ClockAt(c, FtU(f));
    SYSTEMTIME base = zt.st; base.wHour = base.wMinute = base.wSecond = base.wMilliseconds = 0;
    FILETIME bf; SystemTimeToFileTime(&base, &bf);
    FILETIME df = UFt(FtU(bf) + (ULONGLONG)(day * 1440LL * (long long)TPM));
    SYSTEMTIME lt; FileTimeToSystemTime(&df, &lt); lt.wHour = (WORD)hh; lt.wMinute = (WORD)mm;
    return LocalToUtcMin(c, lt, target);
}
static void JumpPreview(HWND d, JumpDlg* st) {
    long long t;
    if (!JumpRead(d, st, t)) { SetDlgItemTextW(d, IDC_JPREVIEW, L"Type a time like 15:30, 3:30 pm or 3pm."); return; }
    CTime local = ClockAt(g_localClock, (ULONGLONG)t * TPM); long long dm = t - NowMin();
    std::wstring s = L"= " + std::wstring(kDays[local.st.wDayOfWeek]) + L" " + FmtHMFull(local.st) + L" your time  ·  " +
                     (dm >= 0 ? L"in " + FmtSpan(dm) : FmtSpan(-dm) + L" ago");
    SetDlgItemTextW(d, IDC_JPREVIEW, s.c_str());
}
static INT_PTR CALLBACK JumpDlgProc(HWND d, UINT m, WPARAM wp, LPARAM lp) {
    JumpDlg* st = (JumpDlg*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG: {
        st = (JumpDlg*)lp; SetWindowLongPtrW(d, DWLP_USER, lp); SetDlgIcon(d);
        int sel = 0;
        for (size_t i = 0; i < st->opts.size(); i++) {
            SendDlgItemMessageW(d, IDC_JCLOCK, CB_ADDSTRING, 0, (LPARAM)st->opts[i].first.c_str());
            if (sel == 0 && _wcsicmp(st->opts[i].second.c_str(), L"Local") != 0) sel = (int)i;
        }
        SendDlgItemMessageW(d, IDC_JCLOCK, CB_SETCURSEL, sel, 0);
        const wchar_t* days[] = { L"Yesterday", L"Today", L"Tomorrow", L"In 2 days", L"In 3 days", L"In 4 days", L"In 5 days", L"In 6 days" };
        for (auto x : days) SendDlgItemMessageW(d, IDC_JDAY, CB_ADDSTRING, 0, (LPARAM)x);
        SendDlgItemMessageW(d, IDC_JDAY, CB_SETCURSEL, 1, 0);
        SetDlgItemTextW(d, IDC_JTIME, S.h24 ? L"15:00" : L"3:00 pm");
        JumpPreview(d, st);
        SetForegroundWindow(d); SetFocus(GetDlgItem(d, IDC_JTIME)); SendDlgItemMessageW(d, IDC_JTIME, EM_SETSEL, 0, -1);
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_JTIME: if (HIWORD(wp) == EN_CHANGE) JumpPreview(d, st); break;
        case IDC_JCLOCK: case IDC_JDAY: if (HIWORD(wp) == CBN_SELCHANGE) JumpPreview(d, st); break;
        case IDOK: {
            long long t; if (!JumpRead(d, st, t)) { MessageBeep(MB_ICONWARNING); return TRUE; }
            st->target = t; EndDialog(d, IDOK); return TRUE;
        }
        case IDCANCEL: EndDialog(d, IDCANCEL); return TRUE;
        }
        break;
    }
    return FALSE;
}
static void JumpDialog() {
    static bool open = false; if (open) return; open = true;
    JumpDlg st; st.opts = ZoneOptions();
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_JUMP), g_main, JumpDlgProc, (LPARAM)&st) == IDOK) {
        g_shift = std::clamp(st.target - NowMin(), -14LL * 1440, 14LL * 1440);
        if (!S.visible) ShowWidget(true);
        RefreshAll();
    }
    open = false; TrimMemory();
}

// ---------------------------------------------------------------- meeting finder
static void FindMeetingSlot() {
    if (g_clocks.size() < 2) return;
    long long now = NowMin(), start = now / 15 * 15;
    for (int k = 0; k < 14 * 96; k++) {
        long long u = start + k * 15LL; bool all = true;
        for (auto& c : g_clocks) {
            CTime t = ClockAt(c, (ULONGLONG)u * TPM); int dw = t.st.wDayOfWeek, h = t.st.wHour;
            if (dw == 0 || dw == 6 || h < S.workStart || h >= S.workEnd) { all = false; break; }
        }
        if (!all) continue;
        g_shift = std::max(0LL, u - now);
        if (!S.visible) ShowWidget(true);
        RefreshAll();
        Frame fr = MakeFrame(); std::wstring s;
        for (size_t i = 0; i < g_clocks.size(); i++)
            s += g_clocks[i].label + L"  " + kDays[fr.t[i].st.wDayOfWeek] + L" " + FmtHMFull(fr.t[i].st) + L"\n";
        if (!s.empty()) s.pop_back();
        Balloon(g_shift ? L"Next time everyone is working" : L"Everyone is in working hours now", s);
        return;
    }
    MessageBoxW(nullptr, L"There's no time in the next two weeks when every clock is inside working hours.\n"
                         L"Try wider working hours (Appearance → Working hours) or remove a clock.", L"WinClock", MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------- context menu
enum {
    ID_ADD = 100, ID_LAYOUT = 110, ID_THEME = 120, ID_OPA = 130, ID_SCALE = 140,
    ID_24H = 150, ID_SECS, ID_DATE, ID_ZDESK = 160, ID_ZTOP, ID_LOCK = 165, ID_CLICK,
    ID_TB_SHOW = 170, ID_TB_ALL, ID_TB_ROT, ID_TB_LABELS, ID_TB_RESET, ID_TB_LEFT, ID_TB_RIGHT, ID_TB_CUSTOM, ID_TRAY,
    ID_RESETSHIFT = 180, ID_COPY, ID_TOGGLEVIS, ID_STARTUP, ID_OPENINI, ID_RELOAD, ID_ABOUT, ID_EXIT,
    ID_COLS = 190, ID_FLY = 200, ID_WORK = 210, ID_ALARM_ADD = 230, ID_JUMP, ID_MEET, ID_CHIME,
    ID_CONVERT = 240, ID_CARD = 241, ID_PIN = 242, ID_FAIR = 243, ID_DST = 244, ID_PALETTE = 245, ID_SETTINGS = 246,
    ID_SHARE_EXPORT = 247, ID_SHARE_IMPORT = 248, ID_CAL_CONNECT = 249, ID_CAL_TOGGLE = 250, ID_CAL_REFRESH = 251, ID_CAL_OFF = 252,
    ID_UPD_TOGGLE = 253, ID_UPD_NOW = 254, ID_UPD_INSTALL = 262, ID_PROF_AUTO = 255, ID_PROF_NEW = 256, ID_PROF_DEL = 257,
    ID_TB_WHICH_ALL = 258, ID_TB_WHICH_SEL = 259, ID_TB_ALLMON = 260, ID_TRAYTIME = 261,
    ID_MON = 300, ID_PROF = 320, ID_PIN_ITEM = 400, ID_TBCLOCK = 900, ID_CLOCK = 1000, ID_ALARMS = 2000
};
static const int kOpa[] = { 100, 90, 80, 70, 60, 50, 40 };
static const int kScale[] = { 75, 90, 100, 115, 130, 150, 175, 200 };
static const int kWork[][2] = { {8, 16}, {9, 17}, {9, 18}, {10, 18}, {10, 19} };
static void ShowPalette();
static void SettingsWindow(int startPage);
static void ConvertClipboard();

static HMENU BuildToolsMenu(); static HMENU BuildShowMenu(); static void AddLookMenus(HMENU look); static void AddShareMenus(HMENU share);
static bool HandleExtraCommand(int id); static void TranslateMenu(HMENU m);
static HMENU BuildMenu() {
    HMENU m = CreatePopupMenu();
    auto chk = [](HMENU h, UINT id, const std::wstring& t, bool on, bool radio = false, bool gray = false) {
        MENUITEMINFOW mi{}; mi.cbSize = sizeof(mi); mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
        mi.fType = radio ? MFT_RADIOCHECK : MFT_STRING; mi.fState = (on ? MFS_CHECKED : 0) | (gray ? MFS_GRAYED : 0); mi.wID = id; mi.dwTypeData = (LPWSTR)t.c_str();
        InsertMenuItemW(h, GetMenuItemCount(h), TRUE, &mi);
    };
    auto amp = [](std::wstring s) { std::wstring o; for (wchar_t c : s) { if (c == L'&') o += L"&&"; else o += c; } return o; };
    if (!g_updTag.empty()) { AppendMenuW(m, MF_STRING, ID_UPD_INSTALL, (L"\u2B06  Update to WinClock " + g_updTag + L"…").c_str()); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
    AppendMenuW(m, MF_STRING, ID_ADD, L"➕  Add clocks / countries…");
    HMENU clocks = CreatePopupMenu();
    for (int i = 0; i < (int)g_clocks.size(); i++) {
        HMENU c = CreatePopupMenu();
        AppendMenuW(c, MF_STRING, ID_CLOCK + i * 10 + 0, L"Edit / change zone…");
        AppendMenuW(c, MF_STRING, ID_CLOCK + i * 10 + 5, L"Person, hours && holidays…");
        AppendMenuW(c, MF_STRING, ID_CLOCK + i * 10 + 7, L"Copy this time");
        AppendMenuW(c, MF_SEPARATOR, 0, nullptr);
        chk(c, ID_CLOCK + i * 10 + 1, L"Show on taskbar", S.tbWhich == 0 || g_clocks[i].taskbar, false, S.tbWhich == 0);
        chk(c, ID_CLOCK + i * 10 + 6, L"Use for the tray icon", g_clocks[i].tray && S.trayTime);
        AppendMenuW(c, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(c, MF_STRING | (i == 0 ? MF_GRAYED : 0), ID_CLOCK + i * 10 + 2, L"Move up");
        AppendMenuW(c, MF_STRING | (i + 1 == (int)g_clocks.size() ? MF_GRAYED : 0), ID_CLOCK + i * 10 + 3, L"Move down");
        AppendMenuW(c, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(c, MF_STRING, ID_CLOCK + i * 10 + 4, L"Remove");
        AppendMenuW(clocks, MF_POPUP, (UINT_PTR)c, amp(DisplayName(i)).c_str());
    }
    if (g_clocks.empty()) AppendMenuW(clocks, MF_STRING | MF_GRAYED, 0, L"(none)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)clocks, L"Clocks");
    HMENU prof = CreatePopupMenu();
    auto names = ProfileNames();
    for (int i = 0; i < (int)names.size() && i < 20; i++) chk(prof, ID_PROF + i, amp(names[i]), names[i] == S.profile, true);
    AppendMenuW(prof, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(prof, MF_STRING, ID_PROF_NEW, L"New profile from these clocks…");
    AppendMenuW(prof, MF_STRING | (S.profile == L"Default" ? MF_GRAYED : 0), ID_PROF_DEL, (L"Delete “" + amp(S.profile) + L"”").c_str());
    bool canAuto = std::find(names.begin(), names.end(), L"Work") != names.end() && std::find(names.begin(), names.end(), L"Personal") != names.end();
    chk(prof, ID_PROF_AUTO, canAuto ? L"Switch automatically (Work in work hours, Personal otherwise)" : L"Switch automatically (needs profiles named Work and Personal)", S.autoProfile, false, !canAuto);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)prof, (L"Profile: " + amp(S.profile)).c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    HMENU plan = CreatePopupMenu();
    AppendMenuW(plan, MF_STRING, ID_CONVERT, (L"Convert time from clipboard" + HotkeyTab(S.hkConvert)).c_str());
    AppendMenuW(plan, MF_STRING, ID_JUMP, L"Jump to a time…");
    AppendMenuW(plan, MF_STRING | (g_clocks.size() < 2 ? MF_GRAYED : 0), ID_MEET, L"Find next meeting slot");
    AppendMenuW(plan, MF_STRING, ID_CARD, g_shift ? L"Meeting card / invite for this time…" : L"Meeting card / invite…");
    AppendMenuW(plan, MF_STRING, ID_PIN, g_shift ? L"Pin this planned moment…" : L"Pin this moment…");
    if (!g_pins.empty()) {
        HMENU pins = CreatePopupMenu();
        for (int i = 0; i < (int)g_pins.size() && i < 20; i++) {
            HMENU pm = CreatePopupMenu();
            AppendMenuW(pm, MF_STRING, ID_PIN_ITEM + i * 10 + 0, L"Show it on the clocks");
            AppendMenuW(pm, MF_STRING, ID_PIN_ITEM + i * 10 + 1, L"Copy meeting card");
            AppendMenuW(pm, MF_STRING, ID_PIN_ITEM + i * 10 + 2, L"Remove");
            AppendMenuW(pins, MF_POPUP, (UINT_PTR)pm, (amp(g_pins[i].label) + L"   (" + PinCountdown(g_pins[i]) + L")").c_str());
        }
        AppendMenuW(plan, MF_POPUP, (UINT_PTR)pins, L"Pinned moments");
    }
    AppendMenuW(plan, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(plan, MF_STRING, ID_FAIR, L"Fairness report…");
    AppendMenuW(plan, MF_STRING, ID_DST, L"Daylight-saving radar…");
    AppendMenuW(plan, MF_STRING, ID_PALETTE, (L"Time calculator…" + HotkeyTab(S.hkPalette)).c_str());
    AppendMenuW(m, MF_POPUP, (UINT_PTR)plan, L"Plan");
    HMENU al = CreatePopupMenu();
    AppendMenuW(al, MF_STRING, ID_ALARM_ADD, L"Add alarm or countdown…");
    if (!g_alarms.empty()) AppendMenuW(al, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < (int)g_alarms.size(); i++) {
        HMENU a = CreatePopupMenu();
        AppendMenuW(a, MF_STRING, ID_ALARMS + i * 10 + 0, L"Edit…");
        chk(a, ID_ALARMS + i * 10 + 1, L"Enabled", g_alarms[i].enabled);
        AppendMenuW(a, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(a, MF_STRING, ID_ALARMS + i * 10 + 2, L"Delete");
        AppendMenuW(al, MF_POPUP, (UINT_PTR)a, AlarmMenuName(i).c_str());
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)al, L"Alarms && countdowns");
    HMENU cal = CreatePopupMenu();
    AppendMenuW(cal, MF_STRING, ID_CAL_CONNECT, S.calUrl.empty() ? L"Connect a calendar link (.ics)…" : L"Change calendar link…");
    chk(cal, ID_CAL_TOGGLE, L"Show next meeting", S.calOn, false, S.calUrl.empty());
    AppendMenuW(cal, MF_STRING | (S.calOn ? 0 : MF_GRAYED), ID_CAL_REFRESH, L"Refresh now");
    AppendMenuW(cal, MF_STRING | (S.calUrl.empty() ? MF_GRAYED : 0), ID_CAL_OFF, L"Disconnect");
    if (!g_calError.empty() && S.calOn) AppendMenuW(cal, MF_STRING | MF_GRAYED, 0, (L"⚠ " + g_calError).c_str());
    AppendMenuW(m, MF_POPUP, (UINT_PTR)cal, L"Calendar");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)BuildToolsMenu(), L"Tools");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)BuildShowMenu(), L"Show");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    HMENU lay = CreatePopupMenu();
    for (int i = 0; i < L_COUNT; i++) chk(lay, ID_LAYOUT + i, kLayoutName[i], S.layout == i, true);
    AppendMenuW(lay, MF_SEPARATOR, 0, nullptr);
    HMENU cols = CreatePopupMenu();
    for (int i = 1; i <= 6; i++) chk(cols, ID_COLS + i, std::to_wstring(i), S.cols == i, true);
    AppendMenuW(lay, MF_POPUP, (UINT_PTR)cols, L"Analog columns");
    HMENU fly = CreatePopupMenu();
    for (int i = 0; i < L_COUNT; i++) chk(fly, ID_FLY + i, kLayoutName[i], S.flyLayout == i, true);
    AppendMenuW(lay, MF_POPUP, (UINT_PTR)fly, L"Taskbar pop-up layout");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)lay, L"Layout");

    HMENU th = CreatePopupMenu();
    for (int i = 0; i < T_COUNT; i++) chk(th, ID_THEME + i, kThemeName[i], S.theme == i, true);
    if (g_highContrast) AppendMenuW(th, MF_STRING | MF_GRAYED, 0, L"(High contrast is on, so Windows colours are used)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)th, L"Theme");

    HMENU look = CreatePopupMenu();
    HMENU sz = CreatePopupMenu();
    for (int i = 0; i < (int)(sizeof(kScale) / sizeof(*kScale)); i++) chk(sz, ID_SCALE + i, std::to_wstring(kScale[i]) + L"%", S.scale == kScale[i], true);
    AppendMenuW(look, MF_POPUP, (UINT_PTR)sz, L"Size  (Ctrl + wheel)");
    HMENU op = CreatePopupMenu();
    for (int i = 0; i < (int)(sizeof(kOpa) / sizeof(*kOpa)); i++) chk(op, ID_OPA + i, std::to_wstring(kOpa[i]) + L"%", S.opacity == kOpa[i], true);
    AppendMenuW(look, MF_POPUP, (UINT_PTR)op, L"Opacity");
    AppendMenuW(look, MF_SEPARATOR, 0, nullptr);
    chk(look, ID_24H, L"24-hour time", S.h24); chk(look, ID_SECS, L"Show seconds", S.secs); chk(look, ID_DATE, L"Show dates", S.date);
    chk(look, ID_CHIME, L"Hourly chime", S.chime);
    HMENU wk = CreatePopupMenu();
    for (int i = 0; i < 5; i++) chk(wk, ID_WORK + i, Fmt(L"%02d:00 – %02d:00", kWork[i][0], kWork[i][1]), S.workStart == kWork[i][0] && S.workEnd == kWork[i][1], true);
    AppendMenuW(look, MF_POPUP, (UINT_PTR)wk, L"Default working hours");
    AddLookMenus(look);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)look, L"Appearance");

    HMENU pos = CreatePopupMenu();
    chk(pos, ID_ZDESK, L"Pin to desktop", S.zmode == Z_DESKTOP, true);
    chk(pos, ID_ZTOP, L"Always on top (hides during full-screen apps)", S.zmode == Z_TOP, true);
    AppendMenuW(pos, MF_SEPARATOR, 0, nullptr);
    chk(pos, ID_LOCK, L"Lock position", S.locked);
    chk(pos, ID_CLICK, L"Click-through (use tray icon to undo)", S.clickThrough);
    auto mons = MonitorWorkAreas();
    if (mons.size() > 1) {
        HMENU mm = CreatePopupMenu();
        for (int i = 0; i < (int)mons.size() && i < 8; i++) AppendMenuW(mm, MF_STRING, ID_MON + i, Fmt(L"Monitor %d%ls", i + 1, i == 0 ? L" (main)" : L"").c_str());
        AppendMenuW(pos, MF_POPUP, (UINT_PTR)mm, L"Move to monitor");
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)pos, L"Desktop widget");

    HMENU tb = CreatePopupMenu();
    chk(tb, ID_TB_SHOW, L"Show clocks on taskbar", S.tbShow);
    AppendMenuW(tb, MF_SEPARATOR, 0, nullptr);
    chk(tb, ID_TB_WHICH_ALL, L"Show all my clocks", S.tbWhich == 0, true);
    chk(tb, ID_TB_WHICH_SEL, L"Show only the ticked clocks:", S.tbWhich == 1, true);
    for (int i = 0; i < (int)g_clocks.size() && i < 50; i++) chk(tb, ID_TBCLOCK + i, L"      " + amp(DisplayName(i)), S.tbWhich == 0 || g_clocks[i].taskbar, false, S.tbWhich == 0);
    AppendMenuW(tb, MF_SEPARATOR, 0, nullptr);
    chk(tb, ID_TB_ALL, L"Side by side (rotates if they don't fit)", !S.tbRotate, true);
    chk(tb, ID_TB_ROT, L"One at a time, rotating", S.tbRotate, true);
    chk(tb, ID_TB_LABELS, L"Show city labels", S.tbLabels);
    AppendMenuW(tb, MF_SEPARATOR, 0, nullptr);
    chk(tb, ID_TB_LEFT, L"Position: left edge", S.tbAnchor == 1, true);
    chk(tb, ID_TB_RIGHT, L"Position: right, next to tray", S.tbAnchor == 0, true);
    chk(tb, ID_TB_CUSTOM, L"Position: custom (drag the clocks)", S.tbAnchor == 2, true);
    chk(tb, ID_TB_ALLMON, L"Also on other monitors' taskbars", S.tbAllMon);
    AppendMenuW(tb, MF_SEPARATOR, 0, nullptr);
    chk(tb, ID_TRAY, L"Show tray icon", S.trayIcon);
    chk(tb, ID_TRAYTIME, L"Tray icon shows a city's hour", S.trayTime, false, !S.trayIcon);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)tb, L"Taskbar");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(m, MF_STRING, ID_PALETTE, (L"Command palette…" + HotkeyTab(S.hkPalette)).c_str());
    AppendMenuW(m, MF_STRING, ID_SETTINGS, L"Settings…");
    HMENU share = CreatePopupMenu();
    AppendMenuW(share, MF_STRING, ID_SHARE_EXPORT, L"Copy share code for these clocks");
    AppendMenuW(share, MF_STRING, ID_SHARE_IMPORT, L"Import a share code…");
    AddShareMenus(share);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)share, L"Share");
    if (g_shift) AppendMenuW(m, MF_STRING, ID_RESETSHIFT, (L"Back to now (shifted " + FmtShift() + L")").c_str());
    AppendMenuW(m, MF_STRING, ID_COPY, L"Copy all times");
    AppendMenuW(m, MF_STRING, ID_TOGGLEVIS, (std::wstring(S.visible ? L"Hide widget" : L"Show widget") + HotkeyTab(S.hkShow)).c_str());
#ifdef STORE_BUILD
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"Start with Windows: Windows Settings \u2192 Apps \u2192 Startup");
#else
    chk(m, ID_STARTUP, L"Start with Windows", StartupEnabled());
#endif
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU help = CreatePopupMenu();
#ifdef STORE_BUILD
    AppendMenuW(help, MF_STRING | MF_GRAYED, 0, L"Updates come from the Microsoft Store");
#else
    chk(help, ID_UPD_TOGGLE, L"Check for updates when WinClock starts", S.updates);
    AppendMenuW(help, MF_STRING, ID_UPD_NOW, L"Check for updates now");
#endif
    AppendMenuW(help, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(help, MF_STRING, ID_OPENINI, L"Open settings file");
    AppendMenuW(help, MF_STRING, ID_RELOAD, L"Reload settings");
    AppendMenuW(help, MF_STRING, ID_ABOUT, L"About WinClock");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)help, L"Help && updates");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");
    return m;
}

static void CopyTimes() {
    Frame fr = MakeFrame(); std::wstring s;
    for (size_t i = 0; i < g_clocks.size(); i++) {
        const CTime& t = fr.t[i];
        s += Fmt(L"%ls — %ls %d %ls, %ls (%ls)\r\n", DisplayName((int)i).c_str(), kDays[t.st.wDayOfWeek], t.st.wDay, kMonths[t.st.wMonth - 1],
                 FmtHMFull(t.st).c_str(), FmtUtc(t.offset).c_str());
    }
    CopyText(s);
}
static void ShowFlyoutAlways();

static void HandleCommand(int id) {
    bool save = true, clocksChanged = false;
    if (HandleExtraCommand(id)) return;
    if (id == ID_ADD) { ClockDialog(-1); ComputeDst(true); return; }
    if (id == ID_ALARM_ADD) { AlarmDialog(-1); return; }
    if (id == ID_JUMP) { JumpDialog(); return; }
    if (id == ID_MEET) { FindMeetingSlot(); return; }
    if (id >= ID_ALARMS) {
        int i = (id - ID_ALARMS) / 10, a = (id - ID_ALARMS) % 10;
        if (i >= (int)g_alarms.size()) return;
        if (a == 0) { AlarmDialog(i); return; }
        if (a == 1) { g_alarms[i].enabled = !g_alarms[i].enabled; g_alarms[i].next = -1; if (g_alarms[i].enabled) ComputeNext(g_alarms[i], NowMin()); }
        if (a == 2) g_alarms.erase(g_alarms.begin() + i);
        SaveAlarms(); RefreshAll(); return;
    }
    else if (id >= ID_CLOCK) {
        int i = (id - ID_CLOCK) / 10, a = (id - ID_CLOCK) % 10;
        if (i >= (int)g_clocks.size()) return;
        if (a == 0) { ClockDialog(i); ComputeDst(true); return; }
        if (a == 5) { PersonDialog(i); RefreshAll(); return; }
        if (a == 7) { Frame fr = MakeFrame(); const CTime& t = fr.t[i];
                      CopyText(DisplayName(i) + L": " + FmtHMFull(t.st) + L" " + kDays[t.st.wDayOfWeek] + Fmt(L" %d %ls", t.st.wDay, kMonths[t.st.wMonth - 1])); return; }
        if (a == 6) { bool on = !(g_clocks[i].tray && S.trayTime); for (auto& c : g_clocks) c.tray = false; g_clocks[i].tray = on; S.trayTime = on || S.trayTime; if (!on) S.trayTime = false; }
        if (a == 1) g_clocks[i].taskbar = !g_clocks[i].taskbar;
        if (a == 2 && i > 0) std::swap(g_clocks[i], g_clocks[i - 1]);
        if (a == 3 && i + 1 < (int)g_clocks.size()) std::swap(g_clocks[i], g_clocks[i + 1]);
        if (a == 4) g_clocks.erase(g_clocks.begin() + i);
        clocksChanged = true; g_bar.rot = 0;
    }
    else if (id >= ID_TBCLOCK && id < ID_TBCLOCK + 50) { int i = id - ID_TBCLOCK; if (i < (int)g_clocks.size()) g_clocks[i].taskbar = !g_clocks[i].taskbar; clocksChanged = true; }
    else if (id >= ID_PIN_ITEM && id < ID_PIN_ITEM + 200) {
        int i = (id - ID_PIN_ITEM) / 10, a = (id - ID_PIN_ITEM) % 10;
        if (i >= (int)g_pins.size()) return;
        if (a == 0) { g_shift = g_pins[i].utc - NowMin(); ShowFlyoutAlways(); return; }
        if (a == 1) { CopyText(CRLF(MeetingCard(g_pins[i].label, g_pins[i].utc, 30))); return; }
        if (a == 2) { g_pins.erase(g_pins.begin() + i); SavePins(); }
        save = false;
    }
    else if (id >= ID_PROF && id < ID_PROF + 20) { auto n = ProfileNames(); if (id - ID_PROF < (int)n.size()) { S.autoProfile = false; SwitchProfile(n[id - ID_PROF]); } }
    else if (id >= ID_MON && id < ID_MON + 8) {
        auto mons = MonitorWorkAreas(); int i = id - ID_MON;
        if (i < (int)mons.size()) { S.x = mons[i].right - g_widget.W - 28; S.y = mons[i].top + 28; PlaceWidget(); }
    }
    else if (id >= ID_LAYOUT && id < ID_LAYOUT + L_COUNT) S.layout = id - ID_LAYOUT;
    else if (id >= ID_THEME && id < ID_THEME + T_COUNT) S.theme = id - ID_THEME;
    else if (id >= ID_OPA && id < ID_OPA + 7) S.opacity = kOpa[id - ID_OPA];
    else if (id >= ID_SCALE && id < ID_SCALE + 8) S.scale = kScale[id - ID_SCALE];
    else if (id >= ID_COLS + 1 && id <= ID_COLS + 6) S.cols = id - ID_COLS;
    else if (id >= ID_FLY && id < ID_FLY + L_COUNT) S.flyLayout = id - ID_FLY;
    else if (id >= ID_WORK && id < ID_WORK + 5) { S.workStart = kWork[id - ID_WORK][0]; S.workEnd = kWork[id - ID_WORK][1]; }
    else switch (id) {
        case ID_CHIME: S.chime = !S.chime; break;
        case ID_24H: S.h24 = !S.h24; break;
        case ID_SECS: S.secs = !S.secs; break;
        case ID_DATE: S.date = !S.date; break;
        case ID_ZDESK: S.zmode = Z_DESKTOP; ApplyWidgetStyle(); break;
        case ID_ZTOP: S.zmode = Z_TOP; ApplyWidgetStyle(); break;
        case ID_LOCK: S.locked = !S.locked; break;
        case ID_CLICK: S.clickThrough = !S.clickThrough; ApplyWidgetStyle(); break;
        case ID_TB_SHOW: S.tbShow = !S.tbShow; break;
        case ID_TB_WHICH_ALL: S.tbWhich = 0; break;
        case ID_TB_WHICH_SEL:
            S.tbWhich = 1;
            if (std::none_of(g_clocks.begin(), g_clocks.end(), [](const Clock& c) { return c.taskbar; })) for (auto& c : g_clocks) c.taskbar = true;
            clocksChanged = true; break;
        case ID_TB_ALL: S.tbRotate = false; break;
        case ID_TB_ROT: S.tbRotate = true; break;
        case ID_TB_LABELS: S.tbLabels = !S.tbLabels; break;
        case ID_TB_RESET: case ID_TB_RIGHT: S.tbAnchor = 0; break;
        case ID_TB_LEFT: S.tbAnchor = 1; break;
        case ID_TB_CUSTOM: S.tbAnchor = 2; if (S.tbOffset < 0) S.tbOffset = 320; break;
        case ID_TB_ALLMON: S.tbAllMon = !S.tbAllMon; break;
        case ID_TRAY:
            S.trayIcon = !S.trayIcon;
            if (S.trayIcon) AddTrayIcon(); else RemoveTrayIcon();
            if (!S.trayIcon && !S.tbShow && !S.visible) ShowWidget(true);   // never leave WinClock with no way back in
            break;
        case ID_TRAYTIME: S.trayTime = !S.trayTime; break;
        case ID_RESETSHIFT: g_shift = 0; g_conv.on = false; save = false; break;
        case ID_CONVERT: ConvertClipboard(); return;
        case ID_CARD: MeetingCardDialog(); return;
        case ID_PIN: PinNow(); save = false; break;
        case ID_FAIR: ShowReport(L"Fairness report", FairnessReport()); return;
        case ID_DST: ShowReport(L"Daylight-saving radar", DstReport()); return;
        case ID_PALETTE: ShowPalette(); return;
        case ID_SETTINGS: SettingsWindow(0); return;
        case ID_SHARE_EXPORT: { std::wstring code = ShareCode(); CopyText(code);
            Balloon(L"Share code copied", Fmt(L"%d clocks. Send it to a teammate; they choose Share setup → Import.", (int)g_clocks.size())); return; }
        case ID_SHARE_IMPORT: ImportShareCode(); RefreshAll(); return;
        case ID_CAL_CONNECT: {
            std::wstring url = S.calUrl;
            if (!InputBox(L"Calendar link", L"Paste your calendar's private .ics link (Google: Settings → your calendar → \"Secret address in iCal format\";\n"
                                            L"Outlook: Calendar settings → Shared calendars → Publish → ICS). It's only stored on this PC.", url)) return;
            if (!url.empty() && url.compare(0, 8, L"https://") != 0 && url.compare(0, 9, L"webcal://") != 0) {
                MessageBoxW(nullptr, L"The link must start with https:// (or webcal://).", L"WinClock", MB_OK | MB_ICONWARNING); return; }
            S.calUrl = url; S.calOn = !url.empty(); g_calEvents.clear(); g_calOcc.clear(); g_calFetched = -1; g_calError.clear();
            SaveSettings(); CalTick(); RefreshAll(); return;
        }
        case ID_CAL_TOGGLE: S.calOn = !S.calOn && !S.calUrl.empty(); g_calFetched = -1; if (S.calOn) CalTick(); break;
        case ID_CAL_REFRESH: g_calFetched = -1; CalTick(); return;
        case ID_CAL_OFF: S.calUrl.clear(); S.calOn = false; g_calEvents.clear(); g_calOcc.clear(); g_calError.clear(); break;
        case ID_UPD_TOGGLE: S.updates = !S.updates; if (S.updates) NetStart(NET_UPDATE, kReleasesApi); break;
        case ID_UPD_INSTALL: OfferUpdate(); return;
        case ID_UPD_NOW: NetStart(NET_UPDATE_MANUAL, kReleasesApi); return;
        case ID_PROF_NEW: NewProfile(); break;
        case ID_PROF_DEL: DeleteProfile(); break;
        case ID_PROF_AUTO: S.autoProfile = !S.autoProfile; AutoProfileTick(); break;
        case ID_COPY: CopyTimes(); return;
        case ID_TOGGLEVIS: ShowWidget(!S.visible); return;
        case ID_STARTUP: SetStartup(!StartupEnabled()); return;
        case ID_OPENINI: ShellExecuteW(nullptr, L"open", L"notepad.exe", (L"\"" + g_ini + L"\"").c_str(), nullptr, SW_SHOWNORMAL); return;
        case ID_RELOAD: LoadSettings(); LoadAlarms(); LoadPins(); ClearHolidayCache(); ComputeDst(true); ApplyWidgetStyle(); RefreshAll(); return;
        case ID_ABOUT:
            MessageBoxW(nullptr,
                (std::wstring(L"WinClock " APP_VERSION L" — world clocks for your desktop and taskbar.\n\n"
                L"• " + HotkeyText(S.hkConvert) + L": convert a time you copied (“3pm EST Thu”) into every clock.\n"
                L"• " + HotkeyText(S.hkPalette) + L": command palette (tokyo, add berlin, 3pm ny, meeting…).\n"
                L"• " + HotkeyText(S.hkShow) + L": show or hide the widget. (Change these in Settings → General.)\n"
                L"• Mouse wheel over the widget: move time ±15 min (Shift: ±1 h); middle-click to go back to now.\n"
                L"• Ctrl + wheel: resize. Hover any clock for details; hover a taskbar clock for its day.\n"
                L"• Plan menu: meeting card & .ics invite, pinned moments, fairness report, daylight-saving radar.\n\n"
                L"Time zones and daylight saving come from Windows. Holidays are built in (lunar dates are approximate).\n"
                L"https://github.com/whileTrueFalse/WinClock\n\nSettings: ") + g_ini).c_str(),
                L"About WinClock", MB_OK | MB_ICONINFORMATION);
            return;
        case ID_EXIT: DestroyWindow(g_main); return;
        default: return;
    }
    if (clocksChanged) { SaveClocks(); ComputeDst(true); }
    if (save) SaveSettings();
    UpdateTrayIconImage(true);
    if (S.visible && g_widget.hwnd) {   // keep the widget's right edge anchored when its width changes
        int oldW = g_widget.W; PaintView(g_widget);
        if (oldW && oldW != g_widget.W) {
            RECT r; GetWindowRect(g_widget.hwnd, &r); RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
            if (r.left + oldW >= wa.right - 40) { S.x = r.left + oldW - g_widget.W; S.y = r.top; PlaceWidget(); SaveSettings(); }
        }
    }
    RefreshAll();
}

static void ShowMenu() {
    POINT pt; GetCursorPos(&pt);
    TipHide(g_widget.tip); TipHide(g_flyout.tip); HidePeek();
    HMENU m = BuildMenu();
    if (S.lang) TranslateMenu(m);
    SetForegroundWindow(g_main);
    int id = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, g_main, nullptr);
    PostMessageW(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);
    if (id) HandleCommand(id);
    TrimMemory();
}

#include "palette.inc"
#include "settingsui.inc"
#include "features.inc"
#include "i18n.inc"

// ---------------------------------------------------------------- flyout
static void ShowFlyoutImpl(bool toggle) {
    View& v = g_flyout;
    if (IsWindowVisible(v.hwnd)) { if (toggle) ShowWindow(v.hwnd, SW_HIDE); else { PaintView(v); SetForegroundWindow(v.hwnd); } return; }
    if (toggle && GetTickCount() - g_flyClosedAt < 250) return;
    HidePeek();
    PaintView(v);
    Bar* bar = (g_lastBar && g_lastBar->visible) ? g_lastBar : (g_bar.visible ? &g_bar : nullptr);
    RECT br;
    if (bar) GetWindowRect(bar->hwnd, &br);
    else {   // no taskbar clocks: rise from the bottom-right corner of the main screen
        RECT wa0; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa0, 0);
        br = { wa0.right - 200, wa0.bottom, wa0.right - 20, wa0.bottom + 1 };
    }
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromRect(&br, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT wa = mi.rcWork; int gap = (int)(10 * DpiOf(g_bar.hwnd) / 96);
    int x = (br.left + br.right) / 2 - v.W / 2, y;
    APPBARDATA abd{}; abd.cbSize = sizeof(abd); SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    if (bar && bar != &g_bar) abd.uEdge = (bar->lastTb.top > (mi.rcMonitor.top + mi.rcMonitor.bottom) / 2) ? ABE_BOTTOM : bar->horiz ? ABE_TOP : ABE_LEFT;
    switch (abd.uEdge) {
    case ABE_TOP: y = wa.top + gap; break;
    case ABE_LEFT: x = wa.left + gap; y = (br.top + br.bottom) / 2 - v.H / 2; break;
    case ABE_RIGHT: x = wa.right - v.W - gap; y = (br.top + br.bottom) / 2 - v.H / 2; break;
    default: y = wa.bottom - v.H - gap; break;
    }
    x = std::clamp(x, (int)wa.left + gap, (int)wa.right - v.W - gap);
    y = std::clamp(y, (int)wa.top + gap, std::max((int)wa.top + gap, (int)wa.bottom - v.H - gap));
    // Appear from where it came from: rise out of the taskbar, critically damped (no overshoot).
    BOOL anim = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
    int off = (int)(14 * DpiOf(g_bar.hwnd) / 96);
    v.animDX = abd.uEdge == ABE_LEFT ? -off : abd.uEdge == ABE_RIGHT ? off : 0;
    v.animDY = abd.uEdge == ABE_TOP ? -off : (abd.uEdge == ABE_LEFT || abd.uEdge == ABE_RIGHT) ? 0 : off;
    v.animX = x; v.animY = y;
    if (anim) {
        v.animAlpha = 0; v.animStart = GetTickCount();
        Present(v.hwnd, v.surf, 0, nullptr);
        SetWindowPos(v.hwnd, HWND_TOPMOST, x + v.animDX, y + v.animDY, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        SetTimer(v.hwnd, 7, 10, nullptr);
    } else {
        v.animAlpha = 255; Present(v.hwnd, v.surf, 255, nullptr);
        SetWindowPos(v.hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    SetForegroundWindow(v.hwnd);
    ScheduleTick();
}
static void ShowFlyout() { ShowFlyoutImpl(true); }
static void ShowFlyoutAlways() { ShowFlyoutImpl(false); }

// ---------------------------------------------------------------- view window proc (widget + flyout)
static void ViewHover(View& v, int x, int y) {
    int hv = -1; bool tl = false; int tlMin = 0;
    if (v.tlBar.right > v.tlBar.left && x >= v.tlBar.left && x < v.tlBar.right && y >= v.tlBar.top && y < v.tlBar.bottom) {
        float w = (float)(v.tlBar.right - v.tlBar.left);
        float m = (x - v.tlBar.left) / w * 1440.f - 720.f; tlMin = (int)lroundf(m / 15.f) * 15; tl = true;
    } else for (auto& h : v.hits) if (x >= h.rc.left && x < h.rc.right && y >= h.rc.top && y < h.rc.bottom) { hv = h.clock; break; }
    bool changed = hv != v.hover || tl != v.tlHover || (tl && tlMin != v.tlMin);
    v.hover = hv; v.tlHover = tl; v.tlMin = tlMin;
    if (!changed) return;
    PaintView(v);
    if (tl) {
        Frame f = MakeFrame(tlMin);
        std::wstring s = L"At " + FmtHMFull(f.local.st) + L" your time (" + (tlMin >= 0 ? L"+" : L"−") + Fmt(L"%dh%02d", std::abs(tlMin) / 60, std::abs(tlMin) % 60) + L")\n";
        for (size_t i = 0; i < g_clocks.size(); i++) {
            std::wstring why; int stt = ClockStatusAt(g_clocks[i], f.t[i].st, &why);
            s += Fmt(L"\n%ls  \u2014  %ls %ls  (%ls)", DisplayName((int)i).c_str(), kDays[f.t[i].st.wDayOfWeek], FmtHMFull(f.t[i].st).c_str(),
                     (why.empty() ? Lower(kStatus[stt]) : why).c_str());
        }
        TipShow(v.tip, s);
    } else if (hv >= 0 || hv <= -100) {
        TipHide(v.tip); SetTimer(v.hwnd, TM_TIP, 450, nullptr);
    } else TipHide(v.tip);
}

static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    View* vp = (View*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) { vp = (View*)((CREATESTRUCTW*)lp)->lpCreateParams; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)vp); vp->hwnd = h; }
    if (!vp) return DefWindowProcW(h, m, wp, lp);
    View& v = *vp;
    switch (m) {
    case WM_MOUSEMOVE:
        if (v.dragging) {
            POINT p; GetCursorPos(&p);
            int dx = p.x - v.dragPt.x, dy = p.y - v.dragPt.y;
            if (!v.moved && std::abs(dx) + std::abs(dy) < 4) return 0;
            v.moved = true;
            int w = v.dragRc.right - v.dragRc.left, hh = v.dragRc.bottom - v.dragRc.top;
            RECT r{ v.dragRc.left + dx, v.dragRc.top + dy, v.dragRc.right + dx, v.dragRc.bottom + dy };
            MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi);
            int snap = (int)(14 * DpiOf(h) / 96); RECT wa = mi.rcWork;   // snap to the work-area edges
            if (std::abs(r.left - wa.left) < snap) r.left = wa.left; else if (std::abs(r.left + w - wa.right) < snap) r.left = wa.right - w;
            if (std::abs(r.top - wa.top) < snap) r.top = wa.top; else if (std::abs(r.top + hh - wa.bottom) < snap) r.top = wa.bottom - hh;
            SetWindowPos(h, nullptr, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (!v.tracking) { TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 }; TrackMouseEvent(&t); v.tracking = true; }
        ViewHover(v, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        v.tracking = false; KillTimer(h, TM_TIP); TipHide(v.tip);
        if (v.hover != -1 || v.tlHover) { v.hover = -1; v.tlHover = false; PaintView(v); }
        return 0;
    case WM_TIMER:
        if (wp == 7) {   // flyout entrance: ease-out (critically damped feel), ~180 ms
            float t = std::min(1.f, (GetTickCount() - v.animStart) / 180.f), e = 1 - (1 - t) * (1 - t) * (1 - t);
            v.animAlpha = (BYTE)(255 * e);
            SetWindowPos(h, nullptr, v.animX + (int)lroundf(v.animDX * (1 - e)), v.animY + (int)lroundf(v.animDY * (1 - e)), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            Present(h, v.surf, v.animAlpha, nullptr);
            if (t >= 1) KillTimer(h, 7);
            return 0;
        }
        if (wp == TM_TIP) {
            KillTimer(h, TM_TIP);
            if (v.hover >= 0 && v.hover < (int)g_clocks.size()) { Frame f = MakeFrame(); TipShow(v.tip, ClockTooltip(v.hover, f)); }
            else if (v.hover <= -200 && v.hover > -210) TipShow(v.tip, L"Click to dismiss this answer card");
            else if (v.hover == -300) TipShow(v.tip, g_focus.phase ? L"Click to pause or resume the focus timer" : L"Click to pause or resume the stopwatch");
            else if (v.hover <= -100 && -100 - v.hover < (int)g_pins.size()) TipShow(v.tip, PinTooltip(-100 - v.hover));
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (v.hover <= -200 && v.hover > -210 && -200 - v.hover < (int)g_cards.size()) {   // dismiss an answer card
            g_cards.erase(g_cards.begin() + (-200 - v.hover)); SaveCards(); v.hover = -1; TipHide(v.tip); RefreshAll(); return 0;
        }
        if (v.hover == -300) { TimerBarClick(); RefreshAll(); return 0; }
        if (PtInRect(&v.flashRc, pt)) {
            if (!g_flash.empty()) g_flash.clear();
            else if (g_conv.on) { CopyText(g_conv.line); g_conv.desc = L"copied \u2713 paste it anywhere"; }
            RefreshAll(); return 0;
        }
    }
        TipHide(v.tip);
        if (!v.fly && !S.locked) {   // start dragging: we move the window ourselves so it goes anywhere
            v.dragging = true; v.moved = false; GetCursorPos(&v.dragPt); GetWindowRect(h, &v.dragRc); SetCapture(h);
        }
        return 0;
    case WM_LBUTTONUP:
        if (v.dragging) {
            v.dragging = false; ReleaseCapture();
            if (v.moved) { RECT r; GetWindowRect(h, &r); S.x = r.left; S.y = r.top; SaveSettings(); KeepOnScreen(h); }
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (v.dragging && (HWND)lp != h) { v.dragging = false; if (v.moved) { RECT r; GetWindowRect(h, &r); S.x = r.left; S.y = r.top; SaveSettings(); } }
        return 0;
    case WM_MBUTTONUP:
        if (g_shift) { g_shift = 0; g_conv.on = false; RefreshAll(); }
        return 0;
    case WM_MOUSEWHEEL: {
        int notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA; if (!notches) return 0;
        if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) {
            if (v.fly) return 0;
            S.scale = std::clamp(S.scale + notches * 10, 50, 250); SaveSettings();
        } else {
            g_shift += notches * ((GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) ? 60 : 15);
            g_shift = std::clamp(g_shift, -14LL * 1440, 14LL * 1440);
        }
        TipHide(v.tip); RefreshAll();
        return 0;
    }
    case WM_MOUSEHWHEEL: {
        int notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        g_shift = std::clamp(g_shift + notches * 60, -14LL * 1440, 14LL * 1440); RefreshAll(); return 0;
    }
    case WM_RBUTTONUP: case WM_CONTEXTMENU: ShowMenu(); return 0;
    case WM_MOVING: {   // snap to work-area edges
        RECT* r = (RECT*)lp; MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi);
        int snap = (int)(14 * DpiOf(h) / 96), w = r->right - r->left, hh = r->bottom - r->top; RECT wa = mi.rcWork;
        if (std::abs(r->left - wa.left) < snap) { r->left = wa.left; r->right = r->left + w; }
        if (std::abs(r->right - wa.right) < snap) { r->right = wa.right; r->left = r->right - w; }
        if (std::abs(r->top - wa.top) < snap) { r->top = wa.top; r->bottom = r->top + hh; }
        if (std::abs(r->bottom - wa.bottom) < snap) { r->bottom = wa.bottom; r->top = r->bottom - hh; }
        return TRUE;
    }
    case WM_EXITSIZEMOVE:
        if (!v.fly) { RECT r; GetWindowRect(h, &r); S.x = r.left; S.y = r.top; SaveSettings(); }
        return 0;
    case WM_WINDOWPOSCHANGING:
        if (!v.fly && S.zmode == Z_DESKTOP && !g_deskRaised) {
            WINDOWPOS* p = (WINDOWPOS*)lp;
            if (!(p->flags & SWP_NOZORDER) && p->hwndInsertAfter != HWND_NOTOPMOST) {
                HWND after = DesktopInsertAfter(h);
                if (after != p->hwndInsertAfter && after != HWND_TOP) p->hwndInsertAfter = after;
            }
        }
        break;
    case WM_ACTIVATE:
        if (v.fly && LOWORD(wp) == WA_INACTIVE) { ShowWindow(h, SW_HIDE); TipHide(v.tip); g_flyClosedAt = GetTickCount(); g_shift = 0; g_conv.on = false; ScheduleTick(); if (S.visible) PaintView(g_widget); TrimMemory(); }
        return 0;
    case WM_KEYDOWN:
        if (v.fly && wp == VK_ESCAPE) ShowWindow(h, SW_HIDE);
        if (v.fly && wp == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) { if (g_conv.on) CopyText(g_conv.line); else CopyTimes(); }
        return 0;
    case WM_DPICHANGED:
        PaintView(v); return 0;
    case WM_MOUSEACTIVATE:
        if (!v.fly) return MA_NOACTIVATE;
        break;
    }
    return DefWindowProcW(h, m, wp, lp);
}

// ---------------------------------------------------------------- taskbar strip window proc
static LRESULT CALLBACK BarProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    Bar* bp = (Bar*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!bp) return DefWindowProcW(h, m, wp, lp);
    Bar& b = *bp;
    switch (m) {
    case WM_MOUSEMOVE: {
        if (!b.tracking) { TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 }; TrackMouseEvent(&t); b.tracking = true; }
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (b.down) {
            POINT sp; GetCursorPos(&sp);
            int dx = sp.x - b.downPt.x, dy = sp.y - b.downPt.y;
            if (!b.dragging && !S.locked && (std::abs(b.horiz ? dx : dy) > 5)) { b.dragging = true; HidePeek(); }
            if (b.dragging) {
                RECT tw = b.lastTb;
                if (b.horiz) { b.lastX = std::clamp(b.downX + dx, (int)tw.left, (int)tw.right - b.W); SetWindowPos(h, nullptr, b.lastX, b.lastY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); }
                else { b.lastY = std::clamp(b.downY + dy, (int)tw.top, (int)tw.bottom - b.H); SetWindowPos(h, nullptr, b.lastX, b.lastY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); }
            }
            return 0;
        }
        int hv = -1; for (size_t k = 0; k < b.cells.size(); k++) if (PtInRect(&b.cells[k], pt)) { hv = (int)k; break; }
        if (hv != b.hover) {
            b.hover = hv; g_lastBar = &b; UpdateBar(true);
            if (hv >= 0) { if (g_peekClock >= 0) ShowPeek(b, hv); else SetTimer(h, TM_TIP, 300, nullptr); }
            else HidePeek();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        b.tracking = false; KillTimer(h, TM_TIP); HidePeek();
        if (b.hover != -1) { b.hover = -1; UpdateBar(true); }
        return 0;
    case WM_TIMER:
        if (wp == TM_TIP) {
            KillTimer(h, TM_TIP);
            if (b.hover >= 0) ShowPeek(b, b.hover);
        }
        return 0;
    case WM_LBUTTONDOWN:
        b.down = true; b.dragging = false; GetCursorPos(&b.downPt); b.downX = b.lastX; b.downY = b.lastY; SetCapture(h); g_lastBar = &b; HidePeek(); KillTimer(h, TM_TIP);
        return 0;
    case WM_LBUTTONUP:
        if (b.down) {
            ReleaseCapture(); b.down = false;
            if (b.dragging) {
                b.dragging = false;
                HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
                float d = (pGetDpiForWindow && tray ? (float)pGetDpiForWindow(tray) : DpiOf(nullptr)) / 96.f;
                RECT tw = b.lastTb;
                S.tbAnchor = 2;
                S.tbOffset = b.horiz ? (int)((tw.right - (b.lastX + b.W)) / d) : (int)((tw.bottom - (b.lastY + b.H)) / d);
                SaveSettings(); UpdateBar(true);
            } else {
                POINT cp{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                if (PtInRect(&b.timerRc, cp)) { TimerBarClick(); RefreshAll(); }
                else ShowFlyout();
            }
        }
        return 0;
    case WM_CAPTURECHANGED: b.down = false; b.dragging = false; return 0;
    case WM_RBUTTONUP: HidePeek(); ShowMenu(); return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_DPICHANGED: UpdateBar(true); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

// ---------------------------------------------------------------- foreground hook (desktop pinning + taskbar z-order)
static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD ev, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (ev != EVENT_SYSTEM_FOREGROUND || !hwnd) return;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() && S.zmode == Z_DESKTOP && S.visible && g_widget.hwnd) {
        wchar_t cls[64] = L""; GetClassNameW(hwnd, cls, 64);
        bool desk = !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Progman");
        if (desk && !g_deskRaised) {   // "Show desktop" (Win+D): keep the widget visible
            g_deskRaised = true;
            SetWindowPos(g_widget.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else if (!desk && g_deskRaised) {
            SetWindowPos(g_widget.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            g_deskRaised = false;
            PinToDesktop(g_widget.hwnd);
        }
    }
    if (S.tbShow && !g_paused) { UpdateBar(false); BarRaise(); }
    if (!g_paused) CheckWidgetFullscreen();
}

// ---------------------------------------------------------------- main (hidden) window proc
static void SetPaused(bool p) {
    if (g_paused == p) return;
    g_paused = p;
    if (p) { KillTimer(g_main, TM_TICK); KillTimer(g_main, TM_BAR); TrimMemory(); }
    else RefreshAll();
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_TASKBARCREATED && WM_TASKBARCREATED) { g_trayAdded = false; if (S.trayIcon) AddTrayIcon(); g_trayIconKey = -1; UpdateTrayIconImage(true); g_bar.key = -1; RefreshAll(); return 0; }
    switch (m) {
    case WM_TIMER:
        if (wp == TM_TICK) { Tick(false); ScheduleTick(); }
        else if (wp == TM_ALARM) AlarmTick();
        else if (wp == 5) { KillTimer(h, 5); if (!S.trayIcon) RemoveTrayIcon(); }
        else if (wp == TM_BAR) {
            {
                APPBARDATA abd{}; abd.cbSize = sizeof(abd);
                int every = (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) ? 16 : 2;   // ~4 s
                auto spin = [&](Bar& b) { if ((S.tbRotate || b.autoRotate) && b.items.size() > 1 && b.hover < 0 && ++b.rotCount >= every) { b.rotCount = 0; b.rot++; } };
                spin(g_bar); for (auto eb : g_extraBars) spin(*eb);
            }
            UpdateBar(false); BarRaise(); CheckWidgetFullscreen();
        }
        return 0;
    case WM_APP_TRAY:
        switch (LOWORD(lp)) {
        case WM_CONTEXTMENU: case WM_RBUTTONUP: ShowMenu(); break;
        case NIN_SELECT: case NIN_KEYSELECT: case WM_LBUTTONUP:
            if (S.clickThrough) { S.clickThrough = false; ApplyWidgetStyle(); SaveSettings(); }
            ShowWidget(!S.visible); break;
        case NIN_BALLOONUSERCLICK: OpenBalloonLink(); break;
        case NIN_BALLOONTIMEOUT: break;
        }
        return 0;
    case WM_APP_SHOW: ShowWidget(true); return 0;
    case WM_APP_NET: HandleNet((NetJob*)lp); RefreshAll(); return 0;
    case WM_HOTKEY:
        if (wp == 2) ConvertClipboard();
        else if (wp == 3) ShowPalette();
        else ShowWidget(!S.visible);
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_SESSION_LOCK) SetPaused(true);
        else if (wp == WTS_SESSION_UNLOCK) SetPaused(false);
        return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_POWERSETTINGCHANGE) {
            auto* ps = (POWERBROADCAST_SETTING*)lp;
            if (ps && IsEqualGUID(ps->PowerSetting, kDisplayState) && ps->DataLength >= sizeof(DWORD)) SetPaused(*(DWORD*)ps->Data == 0);
        } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) { g_bar.key = -1; RefreshAll(); AlarmTick(); }
        return TRUE;
    case WM_TIMECHANGE:
        RefreshLocalZone(); CheckTravel(); for (auto& c : g_clocks) ResolveClock(c);
        for (auto& a : g_alarms) { a.next = -1; if (a.enabled) ComputeNext(a, NowMin()); }
        ComputeDst(true); g_calExpanded = -1; RefreshAll(); return 0;
    case WM_SETTINGCHANGE: case WM_DISPLAYCHANGE: case WM_DWMCOLORIZATIONCOLORCHANGED: case WM_SYSCOLORCHANGE:
        RefreshLocalZone(); RefreshAccessibility(); g_appsLight = -1; g_bar.key = -1; for (auto eb : g_extraBars) eb->key = -1;
        if (S.trayTime) UpdateTrayIconImage(true);
        RefreshAll(); return 0;
    case WM_DESTROY:
        RemoveTrayIcon();
        if (g_hook) UnhookWinEvent(g_hook);
        WTSUnRegisterSessionNotification(h);
        if (g_powerNotify) UnregisterPowerSettingNotification(g_powerNotify);
        UnregisterHotKey(h, 1); UnregisterHotKey(h, 2); UnregisterHotKey(h, 3);
        PalClose(); HidePeek();
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}


#ifdef PREVIEW_BUILD
static void SavePng(Surface& sf, const std::wstring& path) {
    CLSID png = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    G::Bitmap bmp(sf.w, sf.h, sf.w * 4, PixelFormat32bppPARGB, (BYTE*)sf.bits);
    bmp.Save(path.c_str(), &png, nullptr);
}
static void WriteTextFile(const std::wstring& path, const std::wstring& text) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) { std::string u = Utf8(text); DWORD w; WriteFile(f, u.data(), (DWORD)u.size(), &w, nullptr); CloseHandle(f); }
}
static void RunPreviews() {
    wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring dir = p; dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    // a realistic team
    g_clocks.clear();
    auto add = [](const wchar_t* label, const wchar_t* tz, const wchar_t* person, int ws, int we, int off, const wchar_t* note) {
        Clock c; c.label = label; c.tzid = tz; c.person = person; c.ws = ws; c.we = we; c.offMask = off; c.note = note; ResolveClock(c); FillFromAlias(c); g_clocks.push_back(c); };
    add(L"Local", L"Local", L"", -1, -1, 0x41, L"");
    add(L"New York", L"Eastern Standard Time", L"Alex", 8, 16, 0x41, L"");
    add(L"London", L"GMT Standard Time", L"", -1, -1, 0x41, L"prefers mornings");
    add(L"Dubai", L"Arabian Standard Time", L"Priya", 9, 18, (1 << 5) | (1 << 6), L"");
    add(L"Tokyo", L"Tokyo Standard Time", L"", -1, -1, 0x41, L"");
    add(L"Sydney", L"AUS Eastern Standard Time", L"Sam", -1, -1, 0x41, L"");
    g_clocks[4].awayY = 2026; g_clocks[4].awayM = 12; g_clocks[4].awayD = 1; g_clocks[4].note = L"on leave till 1 Dec";
    g_pins = { { L"v2.2 launch", NowMin() + 3 * 1440 + 125 }, { L"Board review", NowMin() + 26 * 60 } };
    g_alarms.clear();
    { Alarm a; a.label = L"Team standup"; a.tzid = L"GMT Standard Time"; a.hh = 9; a.mm = 30; a.rep = 2; ComputeNext(a, NowMin()); g_alarms.push_back(a); }
    { Alarm a; a.label = L"Weekly sync"; a.tzid = L"Eastern Standard Time"; a.hh = 10; a.mm = 0; a.rep = 2; ComputeNext(a, NowMin()); g_alarms.push_back(a); }
    ComputeDst(true);
    Settings keep = S;
    S.theme = T_ELECTRIC;
    for (int l = 0; l < L_COUNT; l++) { S.layout = l; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_" + kLayoutKey[l] + L".png"); }
    S.theme = T_SNOW; S.layout = L_LIST; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_paper_list.png");
    S.theme = T_SNOW; S.layout = L_TIMELINE; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_paper_timeline.png");
    g_highContrast = true; S.layout = L_LIST; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_hc_list.png"); g_highContrast = false;
    S.theme = T_ELECTRIC;
    // planning: 6 pm New York tomorrow, as if converted from the clipboard
    WhenResult w; ParseWhen(L"6pm EST tomorrow", w); g_shift = w.utc - NowMin(); g_conv.on = true; g_conv.line = ConversionLine(w);
    g_conv.desc = L"6:00 pm new york (tue)";
    S.layout = L_LIST; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_convert_list.png");
    S.layout = L_COMPACT; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_convert_compact.png");
    g_conv.on = false; g_shift = 0;
    S = keep; S.tbShow = true; S.tbWhich = 0;
    UpdateBar(true); SavePng(g_bar.surf, dir + L"v_taskbar.png");
    ShowPeek(g_bar, 1); SavePng(g_peekSurf, dir + L"v_peek.png"); HidePeek();
    g_flyout.fly = true; PaintView(g_flyout); SavePng(g_flyout.surf, dir + L"v_flyout.png");
    // tray icon, drawn large so it can be inspected
    {
        int sz = 64; S.trayTime = true; int ci = TrayClock();
        CTime t = ClockAt(g_clocks[ci], (ULONGLONG)NowMin() * TPM);
        Surface sf; EnsureSurface(sf, sz, sz);
        G::Bitmap bmp(sz, sz, sz * 4, PixelFormat32bppPARGB, (BYTE*)sf.bits);
        { G::Graphics g(&bmp); SetupGraphics(g); g.Clear(G::Color(0)); FillRR(g, 0.5f, 0.5f, sz - 1.f, sz - 1.f, sz * 0.22f, NAVY);
          int h = S.h24 ? t.st.wHour : (t.st.wHour % 12 == 0 ? 12 : t.st.wHour % 12); std::wstring txt = std::to_wstring(h);
          g_optScale = 1; float px = sz * (txt.size() == 1 ? 0.80f : 0.66f); DT(g, txt, F(FMS, px), sz / 2.f, sz * 0.5f + px * 0.34f - sz * 0.05f, SNOW, AC);
          G::SolidBrush pk{ G::Color(PINK) }; g.FillRectangle(&pk, 2.f, sz - sz / 12.f - 1.5f, std::max(1.f, (sz - 4) * t.st.wMinute / 60.f), sz / 12.f); }
        SavePng(sf, dir + L"v_trayicon.png"); FreeSurface(sf);
    }
    // 2.5 panels: answer card, focus timer, markets, weather, map, custom theme, mini strip, languages
    {
        Settings keep2 = S; S.layout = L_LIST; S.theme = T_ELECTRIC;
        g_cards = { { L"4:30 PM Tue  \u00b7  Germany", L"when it's 9:00 PM Tue in India  \u00b7  4 h 30 min behind India" } };
        FocusStart(1); g_focus.endTick = GetTickCount64() + 18 * 60000 + 42000;
        S.markets = L"NSE,NYSE,LSE,TSE"; S.showMap = true; S.weather = true;
        long long nowm = NowMin(); int codes[] = { 0, 2, 61, 3, 0, 80 }; float temps[] = { 24, 14, 9, 31, 12, 22 };
        for (size_t i = 0; i < g_clocks.size(); i++) { std::wstring k = WxKey(g_clocks[i]); if (!k.empty()) g_weather[k] = { temps[i % 6], codes[i % 6], nowm }; }
        PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_panels.png");
        FocusStop(); g_sw.running = true; g_sw.startTick = GetTickCount64() - 83400; g_sw.laps = { 41200 };
        S.showMap = false; S.markets.clear(); g_cards.clear();
        S.theme = T_CUSTOM; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_custom.png");
        S.theme = T_ELECTRIC; S.layout = L_STRIP; PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_strip.png");
        UpdateBar(true); SavePng(g_bar.surf, dir + L"v_taskbar_timer.png");
        SwReset(); S.weather = false;
        S.layout = L_LIST; S.lang = 2; ApplyLanguageFonts(); PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_lang_de.png");
        S.lang = 1; ApplyLanguageFonts(); PaintView(g_widget); SavePng(g_widget.surf, dir + L"v_lang_hi.png");
        S.lang = 0; ApplyLanguageFonts();
        CalcAnswer ca; std::wstring calc;
        for (const wchar_t* qq : { L"9:00 pm in india, what time it would be in germany", L"9pm IST + 3h 45m", L"how long until 6pm london", L"hours between 9am ny and 5pm tokyo",
                                   L"9am-5pm india in germany", L"9pm india to usa", L"now + 90 min" }) {
            calc += std::wstring(L"> ") + qq + L"\r\n";
            if (CalcQuery(qq, ca)) for (auto& r : ca.rows) calc += L"   " + r.title + L"   |   " + r.sub + L"\r\n"; else calc += L"   (no answer)\r\n";
        }
        WhenResult wm; ParseWhen(L"9pm IST thursday", wm); calc += L"\r\n" + TimeMessage(wm, {}) + L"\r\n\r\n" + WorkHoursStatus() + L"\r\n";
        WriteTextFile(dir + L"v_calc.txt", calc);
        S = keep2; ApplyWidgetStyle();
    }
    WriteTextFile(dir + L"v_fairness.txt", FairnessReport());
    WriteTextFile(dir + L"v_dst.txt", DstReport());
    WriteTextFile(dir + L"v_card.txt", MeetingCard(L"Design review", NowMin() + 1440, 45));
    WriteTextFile(dir + L"v_share.txt", ShareCode());
    WhenResult w2; ParseWhen(L"Let's do Thursday 3:30pm PT", w2); WriteTextFile(dir + L"v_convert.txt", ConversionLine(w2));
    std::wstring diag = Fmt(L"dpi=%d zones=%d\n", (int)DpiOf(g_widget.hwnd), (int)BuildZoneEntries().size());
    for (int k = 0; k < 8; k++) diag += L"font" + std::to_wstring(k) + L"=" + g_famName[k] + L"\n";
    WriteTextFile(dir + L"diag.txt", diag);
}
#endif

#include "tests.inc"

// ---------------------------------------------------------------- entry
static void ApplyLanguageFonts();
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR cmdLine, int) {
    g_inst = inst;
    bool updated = cmdLine && wcsstr(cmdLine, L"/updated");
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"WinClock.SingleInstance.Marshmallow");
    if (GetLastError() == ERROR_ALREADY_EXISTS && updated && mutex) {   // the old copy is closing; wait for it
        DWORD w = WaitForSingleObject(mutex, 15000);
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) SetLastError(0);
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"WinClockMain", nullptr); if (other) PostMessageW(other, WM_APP_SHOW, 0, 0);
        return 0;
    }
    pGetDpiForWindow = (PFN_GetDpiForWindow)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES }; InitCommonControlsEx(&icc);
    G::GdiplusStartupInput gsi; G::GdiplusStartup(&g_gdipToken, &gsi, nullptr);
    InitFonts();
    ChooseIniPath();
    RefreshLocalZone();
    RefreshAccessibility();
    LoadSettings();
    if (S.lang) ApplyLanguageFonts();
    LoadAlarms();
    LoadPins(); PrunePins(); LoadCards();
#ifdef TEST_BUILD
    g_clocks.clear(); RunTests(); return g_testFail;
#endif
    if (GetPrivateProfileIntW(L"Clocks", L"Count", -1, g_ini.c_str()) < 0 && S.profile == L"Default") { SaveSettings(); SaveClocks(); }

    HICON icon = nullptr; LoadIconMetric(inst, MAKEINTRESOURCEW(IDI_APP), LIM_LARGE, &icon); g_bigIcon = icon;
    WNDCLASSEXW wc{ sizeof(wc) }; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = icon;
    wc.lpfnWndProc = MainProc; wc.lpszClassName = L"WinClockMain"; RegisterClassExW(&wc);
    wc.lpfnWndProc = ViewProc; wc.lpszClassName = L"WinClockView"; RegisterClassExW(&wc);
    wc.lpfnWndProc = BarProc; wc.lpszClassName = L"WinClockBar"; RegisterClassExW(&wc);
    wc.lpfnWndProc = DefWindowProcW; wc.lpszClassName = L"WinClockPeek"; RegisterClassExW(&wc);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    g_main = CreateWindowExW(WS_EX_TOOLWINDOW, L"WinClockMain", L"WinClock", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    ChangeWindowMessageFilterEx(g_main, WM_TASKBARCREATED, MSGFLT_ALLOW, nullptr);

    g_widget.fly = false;
    CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"WinClockView", L"WinClock",
                    WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, inst, &g_widget);
    g_flyout.fly = true;
    CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"WinClockView", L"WinClock",
                    WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, inst, &g_flyout);
    CreateBarWindow(&g_bar);
    TipCreate(g_widget.tip, g_widget.hwnd); TipCreate(g_flyout.tip, g_flyout.hwnd); TipCreate(g_bar.tip, g_bar.hwnd);

    PaintView(g_widget); PlaceWidget(); ApplyWidgetStyle();
    if (S.visible) ShowWindow(g_widget.hwnd, SW_SHOWNOACTIVATE);
    ApplyWidgetStyle();

    if (S.trayIcon) AddTrayIcon();
    g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    WTSRegisterSessionNotification(g_main, NOTIFY_FOR_THIS_SESSION);
    g_powerNotify = RegisterPowerSettingNotification(g_main, &kDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE);
    RegisterHotkeys(true);
    ComputeDst(true);

#ifdef PREVIEW_BUILD
    RunPreviews(); DestroyWindow(g_main);
#else
    RefreshAll();
    UpdateTrayIconImage(true);
    CalTick(); UpdateTick(); WeatherTick();
    CleanupOldExe(); CheckTravel();
    if (updated) Balloon(L"WinClock is up to date", L"Updated to WinClock " APP_VERSION L". Your clocks and settings are kept.");
    ScheduleAlarmTimer();
    TrimMemory();
#endif

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    SaveSettings();
    FreeSurface(g_widget.surf); FreeSurface(g_flyout.surf); FreeSurface(g_bar.surf); FreeSurface(g_peekSurf);
    for (auto eb : g_extraBars) { FreeSurface(eb->surf); delete eb; }
    if (g_trayTimeIcon) DestroyIcon(g_trayTimeIcon);
    FreeFonts();
    G::GdiplusShutdown(g_gdipToken);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return 0;
}
