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
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <cstdint>

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

BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
}
