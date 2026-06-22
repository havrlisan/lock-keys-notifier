// ==WindhawkMod==
// @id              lock-keys-notifier
// @name            Lock Keys Notifier
// @description     Shows a customizable toast when a lock key (Caps/Num/Scroll/Insert) is toggled
// @version         1.0.0
// @author          Havrlisan
// @github          https://github.com/havrlisan
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lgdiplus -ldwmapi -lwinmm -lgdi32 -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Lock Keys Notifier

Shows a small, customizable toast notification whenever a lock key
(Caps Lock, Num Lock, Scroll Lock, or Insert) is toggled, displaying its new
ON/OFF state. See the project README for full details.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
// - notifyCapsLock: true
//   $name: Notify on Caps Lock
// - notifyNumLock: true
//   $name: Notify on Num Lock
// - notifyScrollLock: true
//   $name: Notify on Scroll Lock
// - notifyInsert: false
//   $name: Notify on Insert
//   $description: Reports the Insert toggle bit. Its meaning (overtype) is app-specific.
// - durationMs: 1500
//   $name: Display duration (ms)
// - monitor: active
//   $name: Target monitor
//   $options:
//   - active: Active monitor
//   - primary: Primary monitor
//   - all: All monitors
// - positionAnchor: bottom-center
//   $name: Position
//   $options:
//   - top-left: Top left
//   - top-center: Top center
//   - top-right: Top right
//   - middle-left: Middle left
//   - center: Center
//   - middle-right: Middle right
//   - bottom-left: Bottom left
//   - bottom-center: Bottom center
//   - bottom-right: Bottom right
// - offsetX: 0
//   $name: Horizontal offset (px)
// - offsetY: 48
//   $name: Vertical offset (px)
// - fadeEnabled: true
//   $name: Fade animation
// - fadeDurationMs: 150
//   $name: Fade duration (ms)
// - soundMode: none
//   $name: Sound
//   $options:
//   - none: No sound
//   - systemDefault: System default sound
//   - custom: Custom WAV file
// - soundFile: ""
//   $name: Custom sound file
//   $description: Path to a .wav file, used when Sound is set to Custom.
// - autoSize: true
//   $name: Auto-size to text
// - width: 220
//   $name: Width (px, when auto-size off)
// - height: 64
//   $name: Height (px, when auto-size off)
// - padding: 16
//   $name: Padding (px)
// - cornerRadius: 8
//   $name: Corner radius (px)
// - backgroundColor: ""
//   $name: Background color
//   $description: Hex like #1e1e1e. Blank follows the system light/dark theme.
// - backgroundOpacity: 90
//   $name: Background opacity (0-100)
// - textColor: ""
//   $name: Text color
//   $description: Hex. Blank follows the system theme.
// - borderColor: ""
//   $name: Border color
//   $description: Hex. Blank means no border.
// - borderThickness: 0
//   $name: Border thickness (px)
// - fontFamily: Segoe UI
//   $name: Font family
// - fontSize: 24
//   $name: Font size (px)
// - fontBold: false
//   $name: Bold text
// - fontItalic: false
//   $name: Italic text
// - showIndicator: true
//   $name: Show state indicator dot
// - capsAccentColor: ""
//   $name: Caps Lock accent color
//   $description: Hex. Blank uses the system accent color.
// - numAccentColor: ""
//   $name: Num Lock accent color
// - scrollAccentColor: ""
//   $name: Scroll Lock accent color
// - insertAccentColor: ""
//   $name: Insert accent color
// - textTemplate: "{key}: {state}"
//   $name: Text template
//   $description: Use {key} and {state} placeholders.
// - labelOn: "ON"
//   $name: ON label
// - labelOff: "OFF"
//   $name: OFF label
// - nameCaps: "Caps Lock"
//   $name: Caps Lock display name
// - nameNum: "Num Lock"
//   $name: Num Lock display name
// - nameScroll: "Scroll Lock"
//   $name: Scroll Lock display name
// - nameInsert: "Insert"
//   $name: Insert display name
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <cstdint>
#include <dwmapi.h>
#include <gdiplus.h>
#include <mmsystem.h>
#include <vector>
#include <algorithm>

// === HELPERS BEGIN === (pure: no Windhawk/GDI deps; extracted for tests)
enum class Anchor {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

inline bool parseHexColor(const std::wstring& in, uint32_t& outArgb) {
    std::wstring s = in;
    if (!s.empty() && s[0] == L'#') s.erase(0, 1);
    if (s.size() != 3 && s.size() != 6 && s.size() != 8) return false;

    auto nibble = [](wchar_t ch, int& v) -> bool {
        if (ch >= L'0' && ch <= L'9') { v = ch - L'0'; return true; }
        if (ch >= L'a' && ch <= L'f') { v = ch - L'a' + 10; return true; }
        if (ch >= L'A' && ch <= L'F') { v = ch - L'A' + 10; return true; }
        return false;
    };

    int vals[8];
    for (size_t i = 0; i < s.size(); ++i)
        if (!nibble(s[i], vals[i])) return false;

    int a = 255, r, g, b;
    if (s.size() == 3) {
        r = vals[0] * 17; g = vals[1] * 17; b = vals[2] * 17;
    } else if (s.size() == 6) {
        r = vals[0] * 16 + vals[1]; g = vals[2] * 16 + vals[3]; b = vals[4] * 16 + vals[5];
    } else { // 8
        a = vals[0] * 16 + vals[1]; r = vals[2] * 16 + vals[3];
        g = vals[4] * 16 + vals[5]; b = vals[6] * 16 + vals[7];
    }
    outArgb = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    return true;
}

inline std::wstring formatTemplate(const std::wstring& tmpl,
                                   const std::wstring& keyName,
                                   const std::wstring& stateLabel) {
    std::wstring out;
    out.reserve(tmpl.size() + keyName.size() + stateLabel.size());
    for (size_t i = 0; i < tmpl.size();) {
        if (tmpl.compare(i, 5, L"{key}") == 0) { out += keyName; i += 5; }
        else if (tmpl.compare(i, 7, L"{state}") == 0) { out += stateLabel; i += 7; }
        else { out += tmpl[i]; ++i; }
    }
    return out;
}

inline RECT computeToastRect(Anchor a, SIZE size, int offsetX, int offsetY, const RECT& wa) {
    int idx = static_cast<int>(a);
    int col = idx % 3;   // 0 left, 1 center, 2 right
    int row = idx / 3;   // 0 top,  1 middle, 2 bottom
    int waW = wa.right - wa.left;
    int waH = wa.bottom - wa.top;

    int x;
    if (col == 0)      x = wa.left + offsetX;
    else if (col == 1) x = wa.left + (waW - size.cx) / 2 + offsetX;
    else               x = wa.right - size.cx - offsetX;

    int y;
    if (row == 0)      y = wa.top + offsetY;
    else if (row == 1) y = wa.top + (waH - size.cy) / 2 + offsetY;
    else               y = wa.bottom - size.cy - offsetY;

    return RECT{ x, y, x + size.cx, y + size.cy };
}
// === HELPERS END ===

enum class MonitorTarget { Active, Primary, All };
enum class SoundMode { None, SystemDefault, Custom };

struct Settings {
    bool notifyCaps, notifyNum, notifyScroll, notifyInsert;
    int durationMs;
    MonitorTarget monitor;
    Anchor anchor;
    int offsetX, offsetY;
    bool fadeEnabled;
    int fadeDurationMs;
    SoundMode soundMode;
    std::wstring soundFile;
    bool autoSize;
    int width, height, padding, cornerRadius;
    std::wstring backgroundColor;
    int backgroundOpacity;
    std::wstring textColor, borderColor;
    int borderThickness;
    std::wstring fontFamily;
    int fontSize;
    bool fontBold, fontItalic;
    bool showIndicator;
    std::wstring capsAccent, numAccent, scrollAccent, insertAccent;
    std::wstring textTemplate, labelOn, labelOff;
    std::wstring nameCaps, nameNum, nameScroll, nameInsert;
};

Settings g_settings;
CRITICAL_SECTION g_settingsCs;

static std::wstring GetStr(PCWSTR name) {
    PCWSTR p = Wh_GetStringSetting(name);
    std::wstring s = p ? p : L"";
    Wh_FreeStringSetting(p);
    return s;
}

static Anchor ParseAnchor(const std::wstring& s) {
    if (s == L"top-left") return Anchor::TopLeft;
    if (s == L"top-center") return Anchor::TopCenter;
    if (s == L"top-right") return Anchor::TopRight;
    if (s == L"middle-left") return Anchor::MiddleLeft;
    if (s == L"center") return Anchor::Center;
    if (s == L"middle-right") return Anchor::MiddleRight;
    if (s == L"bottom-left") return Anchor::BottomLeft;
    if (s == L"bottom-right") return Anchor::BottomRight;
    return Anchor::BottomCenter;
}

static MonitorTarget ParseMonitor(const std::wstring& s) {
    if (s == L"primary") return MonitorTarget::Primary;
    if (s == L"all") return MonitorTarget::All;
    return MonitorTarget::Active;
}

static SoundMode ParseSound(const std::wstring& s) {
    if (s == L"systemDefault") return SoundMode::SystemDefault;
    if (s == L"custom") return SoundMode::Custom;
    return SoundMode::None;
}

bool SystemUsesLightTheme() {
    DWORD value = 1, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        return value != 0;
    }
    return true;
}

uint32_t SystemAccentArgb() {
    DWORD color = 0; BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&color, &opaque))) {
        return 0xFF000000u | (color & 0x00FFFFFFu); // DWM returns 0xAARRGGBB; force opaque
    }
    return 0xFF0078D7u; // Windows default blue
}

void LoadSettings() {
    Settings s{};
    s.notifyCaps   = Wh_GetIntSetting(L"notifyCapsLock");
    s.notifyNum    = Wh_GetIntSetting(L"notifyNumLock");
    s.notifyScroll = Wh_GetIntSetting(L"notifyScrollLock");
    s.notifyInsert = Wh_GetIntSetting(L"notifyInsert");
    s.durationMs   = Wh_GetIntSetting(L"durationMs");
    s.monitor      = ParseMonitor(GetStr(L"monitor"));
    s.anchor       = ParseAnchor(GetStr(L"positionAnchor"));
    s.offsetX      = Wh_GetIntSetting(L"offsetX");
    s.offsetY      = Wh_GetIntSetting(L"offsetY");
    s.fadeEnabled  = Wh_GetIntSetting(L"fadeEnabled");
    s.fadeDurationMs = Wh_GetIntSetting(L"fadeDurationMs");
    s.soundMode    = ParseSound(GetStr(L"soundMode"));
    s.soundFile    = GetStr(L"soundFile");
    s.autoSize     = Wh_GetIntSetting(L"autoSize");
    s.width        = Wh_GetIntSetting(L"width");
    s.height       = Wh_GetIntSetting(L"height");
    s.padding      = Wh_GetIntSetting(L"padding");
    s.cornerRadius = Wh_GetIntSetting(L"cornerRadius");
    s.backgroundColor   = GetStr(L"backgroundColor");
    s.backgroundOpacity = Wh_GetIntSetting(L"backgroundOpacity");
    s.textColor    = GetStr(L"textColor");
    s.borderColor  = GetStr(L"borderColor");
    s.borderThickness = Wh_GetIntSetting(L"borderThickness");
    s.fontFamily   = GetStr(L"fontFamily");
    s.fontSize     = Wh_GetIntSetting(L"fontSize");
    s.fontBold     = Wh_GetIntSetting(L"fontBold");
    s.fontItalic   = Wh_GetIntSetting(L"fontItalic");
    s.showIndicator = Wh_GetIntSetting(L"showIndicator");
    s.capsAccent   = GetStr(L"capsAccentColor");
    s.numAccent    = GetStr(L"numAccentColor");
    s.scrollAccent = GetStr(L"scrollAccentColor");
    s.insertAccent = GetStr(L"insertAccentColor");
    s.textTemplate = GetStr(L"textTemplate");
    s.labelOn      = GetStr(L"labelOn");
    s.labelOff     = GetStr(L"labelOff");
    s.nameCaps     = GetStr(L"nameCaps");
    s.nameNum      = GetStr(L"nameNum");
    s.nameScroll   = GetStr(L"nameScroll");
    s.nameInsert   = GetStr(L"nameInsert");

    EnterCriticalSection(&g_settingsCs);
    g_settings = std::move(s);
    LeaveCriticalSection(&g_settingsCs);
}

using namespace Gdiplus;

#define WM_APP_SHOWTOAST (WM_APP + 1)
#define WM_APP_QUIT      (WM_APP + 2)

enum KeyIndex { KI_Caps, KI_Num, KI_Scroll, KI_Insert, KI_Count };

static const wchar_t* kToastClass = L"LockKeysNotifierToast";

struct ToastWindow {
    HWND hwnd = nullptr;
    int  alpha = 0;          // 0..255 current constant alpha
    int  phase = 0;          // 0 hidden, 1 fade-in, 2 hold, 3 fade-out
    SIZE size{};             // last rendered size
    HBITMAP dib = nullptr;   // premultiplied ARGB DIB
    void* bits = nullptr;
    RECT area{};             // work area this toast was last presented on (for timer repositioning)
};

static std::vector<ToastWindow> g_toasts;   // index 0 for active/primary; one per monitor for "all"
static DWORD  g_workerThreadId = 0;
static HANDLE g_workerThread = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

// Resolve a color setting string to ARGB, falling back to a theme default.
static uint32_t ResolveColor(const std::wstring& s, uint32_t fallback) {
    uint32_t argb;
    if (parseHexColor(s, argb)) return argb;
    return fallback;
}

static const std::wstring& KeyName(const Settings& s, int ki) {
    switch (ki) {
        case KI_Caps:   return s.nameCaps;
        case KI_Num:    return s.nameNum;
        case KI_Scroll: return s.nameScroll;
        default:        return s.nameInsert;
    }
}

static const std::wstring& KeyAccent(const Settings& s, int ki) {
    switch (ki) {
        case KI_Caps:   return s.capsAccent;
        case KI_Num:    return s.numAccent;
        case KI_Scroll: return s.scrollAccent;
        default:        return s.insertAccent;
    }
}

// Render the toast for (keyIndex, isOn) into tw.dib; sets tw.size. Returns false on failure.
static bool RenderToast(ToastWindow& tw, const Settings& s, int keyIndex, bool isOn) {
    bool light = SystemUsesLightTheme();
    uint32_t themeBg   = light ? 0xFFFFFFFFu : 0xFF202020u;
    uint32_t themeText = light ? 0xFF000000u : 0xFFFFFFFFu;
    uint32_t accent    = SystemAccentArgb();

    uint32_t bg     = ResolveColor(s.backgroundColor, themeBg);
    uint32_t fg     = ResolveColor(s.textColor, themeText);
    uint32_t acc    = ResolveColor(KeyAccent(s, keyIndex), accent);
    bool hasBorder  = s.borderThickness > 0;
    uint32_t border = ResolveColor(s.borderColor, accent);

    // Apply background opacity (0..100) to the background alpha.
    int bgA = ((bg >> 24) & 0xFF) * (s.backgroundOpacity < 0 ? 0 : s.backgroundOpacity > 100 ? 100 : s.backgroundOpacity) / 100;
    bg = (uint32_t(bgA) << 24) | (bg & 0x00FFFFFFu);

    std::wstring text = formatTemplate(s.textTemplate, KeyName(s, keyIndex), isOn ? s.labelOn : s.labelOff);

    // Build font.
    int style = (s.fontBold ? FontStyleBold : 0) | (s.fontItalic ? FontStyleItalic : 0);
    FontFamily ff(s.fontFamily.c_str());
    FontFamily def(L"Segoe UI");
    const FontFamily* useFf = ff.IsAvailable() ? &ff : &def;
    Font font(useFf, (REAL)s.fontSize, style, UnitPixel);

    // Measure text using a scratch graphics.
    int dotW = s.showIndicator ? (s.fontSize / 2 + 8) : 0;
    REAL textW = 0, textH = 0;
    {
        HDC screen = GetDC(nullptr);
        Graphics g(screen);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        RectF bounds;
        g.MeasureString(text.c_str(), -1, &font, PointF(0, 0), &bounds);
        textW = bounds.Width; textH = bounds.Height;
        ReleaseDC(nullptr, screen);
    }

    int w, h;
    if (s.autoSize) {
        w = (int)(textW + 0.5f) + dotW + s.padding * 2;
        h = (int)(textH + 0.5f) + s.padding * 2;
    } else {
        w = s.width; h = s.height;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    // (Re)create the DIB if size changed.
    if (tw.size.cx != w || tw.size.cy != h || !tw.dib) {
        if (tw.dib) { DeleteObject(tw.dib); tw.dib = nullptr; tw.bits = nullptr; }
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;        // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        tw.dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &tw.bits, nullptr, 0);
        if (!tw.dib) return false;
        tw.size = SIZE{ w, h };
    }

    HDC memDC = CreateCompatibleDC(nullptr);
    HGDIOBJ oldBmp = SelectObject(memDC, tw.dib);
    {
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.Clear(Color(0, 0, 0, 0));

        REAL radius = (REAL)s.cornerRadius;
        REAL d = radius * 2;
        RectF rc(0.5f, 0.5f, (REAL)w - 1.0f, (REAL)h - 1.0f);
        GraphicsPath path;
        if (radius > 0) {
            path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
            path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 90.0f);
            path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0.0f, 90.0f);
            path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90.0f, 90.0f);
            path.CloseFigure();
        } else {
            path.AddRectangle(rc);
        }

        auto toColor = [](uint32_t a) { return Color((a >> 24) & 0xFF, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF); };
        SolidBrush bgBrush(toColor(bg));
        g.FillPath(&bgBrush, &path);
        if (hasBorder) {
            Pen pen(toColor(border), (REAL)s.borderThickness);
            g.DrawPath(&pen, &path);
        }

        REAL textLeft = (REAL)s.padding;
        if (s.showIndicator) {
            REAL dia = (REAL)s.fontSize / 2;
            REAL cx = (REAL)s.padding;
            REAL cy = ((REAL)h - dia) / 2;
            Color onColor = toColor(acc);
            Color offColor(0xFF, 0x80, 0x80, 0x80);
            SolidBrush dotBrush(isOn ? onColor : offColor);
            g.FillEllipse(&dotBrush, cx, cy, dia, dia);
            textLeft = cx + dia + 8;
        }

        SolidBrush textBrush(toColor(0xFF000000u | (fg & 0x00FFFFFFu)));
        StringFormat fmt;
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetAlignment(StringAlignmentNear);
        RectF layout(textLeft, 0, (REAL)w - textLeft - s.padding, (REAL)h);
        g.DrawString(text.c_str(), -1, &font, layout, &fmt, &textBrush);
    }
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);

    // Premultiply alpha for UpdateLayeredWindow.
    uint8_t* px = (uint8_t*)tw.bits;
    for (int i = 0; i < w * h; ++i) {
        uint8_t a = px[i * 4 + 3];
        px[i * 4 + 0] = (uint8_t)(px[i * 4 + 0] * a / 255);
        px[i * 4 + 1] = (uint8_t)(px[i * 4 + 1] * a / 255);
        px[i * 4 + 2] = (uint8_t)(px[i * 4 + 2] * a / 255);
    }
    return true;
}

static RECT WorkAreaForTarget(MonitorTarget target) {
    HMONITOR mon = nullptr;
    if (target == MonitorTarget::Primary) {
        mon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    } else { // Active: foreground window, else cursor
        HWND fg = GetForegroundWindow();
        if (fg) mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
        if (!mon) {
            POINT pt; GetCursorPos(&pt);
            mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        }
    }
    MONITORINFO mi{ sizeof(mi) };
    if (mon && GetMonitorInfoW(mon, &mi)) return mi.rcWork;
    RECT r{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    return r;
}

struct MonitorList { std::vector<RECT> work; };
static BOOL CALLBACK EnumMonProc(HMONITOR hm, HDC, LPRECT, LPARAM lp) {
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(hm, &mi)) ((MonitorList*)lp)->work.push_back(mi.rcWork);
    return TRUE;
}

// Present a rendered toast window at the work area; applies fade phase alpha.
static void PresentToast(ToastWindow& tw, const RECT& workArea, const Settings& s) {
    RECT r = computeToastRect(s.anchor, tw.size, s.offsetX, s.offsetY, workArea);
    POINT ptPos{ r.left, r.top };
    SIZE szWnd{ tw.size.cx, tw.size.cy };
    POINT ptSrc{ 0, 0 };

    HDC screen = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(memDC, tw.dib);

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = (BYTE)tw.alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(tw.hwnd, screen, &ptPos, &szWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(memDC, old);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screen);
}

#define FADE_TIMER 1
#define HOLD_TIMER 2
#define FADE_TICK_MS 16

static void DoShow(int keyIndex, bool isOn) {
    Settings s;
    EnterCriticalSection(&g_settingsCs);
    s = g_settings;
    LeaveCriticalSection(&g_settingsCs);

    std::vector<RECT> areas;
    if (s.monitor == MonitorTarget::All) {
        MonitorList ml; EnumDisplayMonitors(nullptr, nullptr, EnumMonProc, (LPARAM)&ml);
        areas = ml.work;
    } else {
        areas.push_back(WorkAreaForTarget(s.monitor));
    }
    if (areas.empty()) return;

    // Ensure we have one ToastWindow per area; the windows themselves were created in the worker init.
    for (size_t i = 0; i < g_toasts.size() && i < areas.size(); ++i) {
        ToastWindow& tw = g_toasts[i];
        if (!RenderToast(tw, s, keyIndex, isOn)) continue;
        tw.area = areas[i];
        tw.alpha = s.fadeEnabled ? (tw.phase == 0 ? 0 : tw.alpha) : 255;
        tw.phase = s.fadeEnabled ? 1 : 2;
        PresentToast(tw, areas[i], s);
        ShowWindow(tw.hwnd, SW_SHOWNA);
        KillTimer(tw.hwnd, FADE_TIMER);
        KillTimer(tw.hwnd, HOLD_TIMER);
        if (s.fadeEnabled && tw.alpha < 255) {
            SetTimer(tw.hwnd, FADE_TIMER, FADE_TICK_MS, nullptr);
        } else {
            tw.alpha = 255; tw.phase = 2;
            PresentToast(tw, areas[i], s);
            SetTimer(tw.hwnd, HOLD_TIMER, (UINT)(s.durationMs < 1 ? 1 : s.durationMs), nullptr);
        }
    }

    // Play sound once per toast event.
    if (s.soundMode == SoundMode::SystemDefault) {
        PlaySoundW((LPCWSTR)SND_ALIAS_SYSTEMDEFAULT, nullptr, SND_ALIAS_ID | SND_ASYNC);
    } else if (s.soundMode == SoundMode::Custom && !s.soundFile.empty()) {
        PlaySoundW(s.soundFile.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
    }
}

static LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_APP_SHOWTOAST:
        DoShow((int)wParam, (bool)lParam);
        return 0;
    case WM_TIMER: {
        // Find which toast owns this hwnd.
        ToastWindow* tw = nullptr;
        for (auto& t : g_toasts) if (t.hwnd == hwnd) { tw = &t; break; }
        if (!tw) return 0;
        Settings s;
        EnterCriticalSection(&g_settingsCs);
        s = g_settings;
        LeaveCriticalSection(&g_settingsCs);
        RECT wa = tw->area;
        int stepMs = FADE_TICK_MS;
        int delta = s.fadeDurationMs > 0 ? (255 * stepMs / s.fadeDurationMs) : 255;
        if (delta < 1) delta = 1;

        if (wParam == FADE_TIMER) {
            if (tw->phase == 1) { // fade in
                tw->alpha += delta;
                if (tw->alpha >= 255) {
                    tw->alpha = 255; tw->phase = 2;
                    KillTimer(hwnd, FADE_TIMER);
                    SetTimer(hwnd, HOLD_TIMER, (UINT)(s.durationMs < 1 ? 1 : s.durationMs), nullptr);
                }
                PresentToast(*tw, wa, s);
            } else if (tw->phase == 3) { // fade out
                tw->alpha -= delta;
                if (tw->alpha <= 0) {
                    tw->alpha = 0; tw->phase = 0;
                    KillTimer(hwnd, FADE_TIMER);
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    PresentToast(*tw, wa, s);
                }
            }
        } else if (wParam == HOLD_TIMER) {
            KillTimer(hwnd, HOLD_TIMER);
            if (s.fadeEnabled) {
                tw->phase = 3;
                SetTimer(hwnd, FADE_TIMER, FADE_TICK_MS, nullptr);
            } else {
                tw->phase = 0; tw->alpha = 0;
                ShowWindow(hwnd, SW_HIDE);
            }
        }
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static HMODULE GetThisModule() {
    HMODULE h = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&GetThisModule, &h);
    return h;
}

void RequestToast(int keyIndex, bool isOn) {
    // Marshal to the worker thread; the worker window proc does the work.
    if (!g_toasts.empty() && g_toasts[0].hwnd) {
        PostMessageW(g_toasts[0].hwnd, WM_APP_SHOWTOAST, (WPARAM)keyIndex, (LPARAM)isOn);
    }
}

static DWORD WINAPI WorkerThreadProc(LPVOID) {
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);

    HMODULE hInst = GetThisModule();
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ToastWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kToastClass;
    RegisterClassExW(&wc);

    // Create up to GetSystemMetrics(SM_CMONITORS) windows so "all monitors" mode has enough.
    int n = GetSystemMetrics(SM_CMONITORS);
    if (n < 1) n = 1;
    g_toasts.resize(n);
    for (auto& tw : g_toasts) {
        tw.hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            kToastClass, L"", WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_APP_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (auto& tw : g_toasts) {
        if (tw.hwnd) DestroyWindow(tw.hwnd);
        if (tw.dib) DeleteObject(tw.dib);
    }
    g_toasts.clear();
    UnregisterClassW(kToastClass, hInst);
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}

bool StartWorker() {
    g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0, &g_workerThreadId);
    return g_workerThread != nullptr;
}

void StopWorker() {
    if (g_workerThreadId)
        PostThreadMessageW(g_workerThreadId, WM_APP_QUIT, 0, 0);
    if (g_workerThread) {
        WaitForSingleObject(g_workerThread, 5000);
        CloseHandle(g_workerThread);
        g_workerThread = nullptr;
        g_workerThreadId = 0;
    }
}

// TEMPORARY (Task 4 verification) — replaced in Task 6.
static HHOOK g_tempHook = nullptr;
static LRESULT CALLBACK TempKbdProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
        if (k->vkCode == 'L' &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_MENU) & 0x8000) &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            static bool on = false; on = !on;
            RequestToast(KI_Caps, on);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    InitializeCriticalSection(&g_settingsCs);
    LoadSettings();
    if (!StartWorker()) { Wh_Log(L"worker start failed"); return FALSE; }
    Sleep(50); // let the worker create its windows
    g_tempHook = SetWindowsHookExW(WH_KEYBOARD_LL, TempKbdProc, GetThisModule(), 0);
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
    if (g_tempHook) { UnhookWindowsHookEx(g_tempHook); g_tempHook = nullptr; }
    StopWorker();
    DeleteCriticalSection(&g_settingsCs);
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"Lock Keys Notifier settings changed");
    LoadSettings();
}
