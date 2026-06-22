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

BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    InitializeCriticalSection(&g_settingsCs);
    LoadSettings();
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
    DeleteCriticalSection(&g_settingsCs);
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"Lock Keys Notifier settings changed");
    LoadSettings();
}
