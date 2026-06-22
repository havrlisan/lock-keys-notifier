# Lock Keys Notifier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windhawk mod that shows a customizable toast notification whenever a lock key (Caps/Num/Scroll/Insert) is toggled, displaying its new ON/OFF state.

**Architecture:** A single self-contained `lock-keys-notifier.wh.cpp` injected into `explorer.exe`. A dedicated worker thread owns a low-level keyboard hook (`WH_KEYBOARD_LL`), a click-through GDI+ layered window, and a message pump. The hook detects key-down edges, flips a self-tracked ON/OFF state, and posts a message to render the toast. Pure helper functions (color parsing, text templating, anchor math) live in a delimited section that is extracted into a tiny test harness and verified with the bundled clang++.

**Tech Stack:** C++23, Win32, GDI+, Windhawk API. Compiler: the LLVM/clang toolchain bundled with Windhawk.

## Global Constraints

- **Mod ID:** `lock-keys-notifier` — **Name:** `Lock Keys Notifier` — **Author:** `Havrlisan` — **GitHub:** `https://github.com/havrlisan` — **License:** MIT, `Copyright (c) 2026 Havrlisan`.
- **Single publishable file:** `lock-keys-notifier.wh.cpp` must be fully self-contained (no local `#include`s). It is the authoritative source; the test harness derives its input *from* it, never the reverse.
- **Host:** `explorer.exe` only. **Architecture:** `x86-64`.
- **Compiler binary (absolute path, has spaces — always quote):** `C:\Program Files\Windhawk\Compiler\bin\clang++.exe`
- **Whole-mod compile gate (run from repo root `T:\lock-keys-notifier`):**
  ```bash
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
  ```
  Expected: no output, exit code 0. (`compile_flags.txt` already sets `-std=c++23 -target x86_64-w64-mingw32 -DUNICODE -DWH_MOD -DWH_EDITING -include windhawk_api.h`.)
- **Pure helper functions** must depend only on `<windows.h>`, `<string>`, `<cstdint>` — never on Windhawk APIs or GDI+ — so they compile standalone.
- **Color settings** are hex strings (`#rgb` / `#rrggbb` / `#aarrggbb`); blank means "derive from system theme".
- **String settings** obtained via `Wh_GetStringSetting` must be freed with `Wh_FreeStringSetting` (use the `GetStr` helper from Task 3).
- Commit after every task. Keep commit subjects short.

## File Structure

| File | Responsibility |
|------|----------------|
| `lock-keys-notifier.wh.cpp` | The mod: metadata/readme/settings blocks, pure helpers (delimited), settings, worker thread, hook, window, rendering, lifecycle. |
| `tools/extract-helpers.sh` | Slices the delimited helper section out of the `.wh.cpp` into `tests/helpers_generated.h`. |
| `tests/helpers_test.cpp` | Assertion-based tests for the pure helpers. |
| `tests/helpers_generated.h` | **Generated** (gitignored) — extracted helper source for tests. |
| `README.md` | GitHub-facing readme (features, settings, caveats, license, install). |
| `LICENSE` | MIT license text. |
| `.gitignore` | Ignores build artifacts (`*.exe`, `tests/helpers_generated.h`). |

---

### Task 1: Scaffolding + compiling skeleton

**Files:**
- Create: `LICENSE`
- Create: `.gitignore`
- Create: `lock-keys-notifier.wh.cpp`
- Create: `README.md` (skeleton)

**Interfaces:**
- Produces: a compiling `.wh.cpp` with the three Windhawk blocks and the lifecycle stubs `Wh_ModInit` (returns `TRUE`) and `Wh_ModUninit` (empty).

- [ ] **Step 1: Create `LICENSE` (MIT)**

```text
MIT License

Copyright (c) 2026 Havrlisan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 2: Create `.gitignore`**

```text
*.exe
*.o
*.obj
tests/helpers_generated.h
```

- [ ] **Step 3: Create `README.md` skeleton** (filled out in Task 7)

```markdown
# Lock Keys Notifier

A [Windhawk](https://github.com/ramensoftware/windhawk) mod that shows a small,
customizable toast notification whenever a lock key (Caps Lock, Num Lock,
Scroll Lock, or Insert) is toggled.

_Full documentation is filled in during Task 7._
```

- [ ] **Step 4: Create `lock-keys-notifier.wh.cpp` skeleton**

```cpp
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

BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
}
```

- [ ] **Step 5: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0. (`Wh_Log` and `Wh_ModInit`/`Wh_ModUninit` resolve via the force-included `windhawk_api.h`.)

- [ ] **Step 6: Commit**

```bash
git add LICENSE .gitignore README.md lock-keys-notifier.wh.cpp
git commit -m "Scaffold mod skeleton, license, gitignore"
```

---

### Task 2: Pure helpers (TDD)

Three deterministic helpers plus the `Anchor` enum, written test-first and verified with clang++. They live inside a delimited section of the `.wh.cpp` so the test harness can extract them.

**Files:**
- Create: `tools/extract-helpers.sh`
- Create: `tests/helpers_test.cpp`
- Modify: `lock-keys-notifier.wh.cpp` (add the `// === HELPERS BEGIN/END ===` section)

**Interfaces:**
- Produces (used by later tasks):
  - `enum class Anchor { TopLeft, TopCenter, TopRight, MiddleLeft, Center, MiddleRight, BottomLeft, BottomCenter, BottomRight };`
  - `bool parseHexColor(const std::wstring& s, uint32_t& outArgb);` — returns `false` for empty/invalid; on success `outArgb` is `0xAARRGGBB` (alpha defaults to `0xFF` when not supplied).
  - `std::wstring formatTemplate(const std::wstring& tmpl, const std::wstring& keyName, const std::wstring& stateLabel);` — substitutes every `{key}` and `{state}`.
  - `RECT computeToastRect(Anchor a, SIZE size, int offsetX, int offsetY, const RECT& workArea);` — offset always pushes inward from the anchored edge; for center axes positive offset shifts right/down.

- [ ] **Step 1: Create the extraction script `tools/extract-helpers.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
out="tests/helpers_generated.h"
{
  echo '#pragma once'
  echo '#include <windows.h>'
  echo '#include <string>'
  echo '#include <cstdint>'
  awk '/\/\/ === HELPERS BEGIN ===/{f=1;next} /\/\/ === HELPERS END ===/{f=0} f' lock-keys-notifier.wh.cpp
} > "$out"
echo "wrote $out"
```

- [ ] **Step 2: Write the failing test `tests/helpers_test.cpp`**

```cpp
#include "helpers_generated.h"
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); ++g_failures; } } while(0)

int main() {
    uint32_t c = 0;
    // parseHexColor
    CHECK(parseHexColor(L"#ff0000", c) && c == 0xFFFF0000u);
    CHECK(parseHexColor(L"ff0000",  c) && c == 0xFFFF0000u);   // '#' optional
    CHECK(parseHexColor(L"#f00",    c) && c == 0xFFFF0000u);   // 3-digit expands
    CHECK(parseHexColor(L"#80ff0000", c) && c == 0x80FF0000u); // explicit alpha
    CHECK(!parseHexColor(L"", c));                              // empty -> false
    CHECK(!parseHexColor(L"nothex", c));                        // invalid -> false
    CHECK(!parseHexColor(L"#12345", c));                        // bad length -> false

    // formatTemplate
    CHECK(formatTemplate(L"{key}: {state}", L"Caps Lock", L"ON") == L"Caps Lock: ON");
    CHECK(formatTemplate(L"{state} {key} {state}", L"Num", L"OFF") == L"OFF Num OFF");
    CHECK(formatTemplate(L"no placeholders", L"X", L"Y") == L"no placeholders");

    // computeToastRect
    RECT wa{0, 0, 1000, 1000};
    SIZE s{100, 40};
    RECT r = computeToastRect(Anchor::TopLeft, s, 0, 0, wa);
    CHECK(r.left == 0 && r.top == 0 && r.right == 100 && r.bottom == 40);
    r = computeToastRect(Anchor::BottomRight, s, 0, 0, wa);
    CHECK(r.left == 900 && r.top == 960 && r.right == 1000 && r.bottom == 1000);
    r = computeToastRect(Anchor::Center, s, 0, 0, wa);
    CHECK(r.left == 450 && r.top == 480);
    r = computeToastRect(Anchor::BottomCenter, s, 0, 10, wa); // offset pushes up from bottom
    CHECK(r.left == 450 && r.top == 950);
    r = computeToastRect(Anchor::TopLeft, s, 5, 7, wa);       // offset pushes in from top-left
    CHECK(r.left == 5 && r.top == 7);

    if (g_failures == 0) { printf("All tests passed\n"); return 0; }
    printf("%d failure(s)\n", g_failures);
    return 1;
}
```

- [ ] **Step 3: Run the test to verify it fails**

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -Itests tests/helpers_test.cpp -o tests/helpers_test.exe
```
Expected: FAIL — compile error (e.g. `use of undeclared identifier 'parseHexColor'` / `'Anchor'`), because the helper section is empty.

- [ ] **Step 4: Add the helper section to `lock-keys-notifier.wh.cpp`**

Insert this block immediately after the `#include <cstdint>` line:

```cpp
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
```

- [ ] **Step 5: Re-extract, compile, and run the tests**

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -Itests tests/helpers_test.cpp -o tests/helpers_test.exe
./tests/helpers_test.exe
```
Expected: `All tests passed`, exit code 0.

- [ ] **Step 6: Run the whole-mod compile gate** (helpers must also compile inside the mod)

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0.

- [ ] **Step 7: Commit**

```bash
git add tools/extract-helpers.sh tests/helpers_test.cpp lock-keys-notifier.wh.cpp
git commit -m "Add tested pure helpers: hex color, template, anchor rect"
```

---

### Task 3: Settings block, struct, and loader

Define every setting in the `==WindhawkModSettings==` block, a `Settings` struct, enums for the choice settings, theme-derivation helpers, and a `LoadSettings()` that fills a global guarded by a `CRITICAL_SECTION`.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp`

**Interfaces:**
- Consumes: `Anchor` (Task 2).
- Produces:
  - `enum class MonitorTarget { Active, Primary, All };`
  - `enum class SoundMode { None, SystemDefault, Custom };`
  - `struct Settings { ... }` (fields listed below).
  - `Settings g_settings;` guarded by `CRITICAL_SECTION g_settingsCs;`
  - `void LoadSettings();` — reads all settings into `g_settings` (caller need not hold the lock; `LoadSettings` takes it internally).
  - `std::wstring GetStr(PCWSTR name);` — reads a string setting and frees it.
  - `bool SystemUsesLightTheme();` and `uint32_t SystemAccentArgb();` — theme helpers (return opaque `0xFFRRGGBB`).

- [ ] **Step 1: Fill the `==WindhawkModSettings==` block**

Replace the empty settings block with:

```cpp
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
```

- [ ] **Step 2: Add includes and the settings struct/globals**

Add to the include list near the top (after `#include <cstdint>`):

```cpp
#include <dwmapi.h>
```

Add this block *after* the `// === HELPERS END ===` marker:

```cpp
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
```

- [ ] **Step 3: Add string/enum readers, theme helpers, and `LoadSettings`**

Add after the globals from Step 2:

```cpp
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
```

- [ ] **Step 4: Initialize the critical section and load settings in `Wh_ModInit`**

Replace the body of `Wh_ModInit` with:

```cpp
BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    InitializeCriticalSection(&g_settingsCs);
    LoadSettings();
    return TRUE;
}
```

Replace `Wh_ModUninit` with:

```cpp
void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
    DeleteCriticalSection(&g_settingsCs);
}
```

Add a settings-changed handler (after `Wh_ModUninit`):

```cpp
void Wh_ModSettingsChanged() {
    Wh_Log(L"Lock Keys Notifier settings changed");
    LoadSettings();
}
```

- [ ] **Step 5: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0.

- [ ] **Step 6: Re-run helper tests** (ensure the new code didn't disturb the helper section)

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: `All tests passed`.

- [ ] **Step 7: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add settings block, struct, loader, and theme helpers"
```

---

### Task 4: Worker thread, layered window, and toast rendering

Create the worker thread that owns a GDI+ click-through layered window and a message pump. Render the toast into a premultiplied ARGB DIB and present it with `UpdateLayeredWindow`. Position it via `computeToastRect` + monitor selection. Implement the fade/hold/dismiss timer state machine. A temporary test trigger (a hotkey) lets you verify rendering before the real hook exists; it is removed in Task 6.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp`

**Interfaces:**
- Consumes: `g_settings`, `g_settingsCs`, `Anchor`, `MonitorTarget`, `computeToastRect`, `parseHexColor`, `formatTemplate`, `SystemUsesLightTheme`, `SystemAccentArgb`.
- Produces:
  - `enum KeyIndex { KI_Caps, KI_Num, KI_Scroll, KI_Insert, KI_Count };`
  - `void RequestToast(int keyIndex, bool isOn);` — thread-safe entry point that the hook (Task 5) calls; posts `WM_APP_SHOWTOAST` to the worker window.
  - `bool StartWorker();` / `void StopWorker();` — lifecycle used by Task 6.
  - Globals: `HWND g_toastWnd[...]`, `DWORD g_workerThreadId`, `HANDLE g_workerThread`.

- [ ] **Step 1: Add GDI+ includes, app messages, and worker globals**

Add to the top includes:

```cpp
#include <gdiplus.h>
#include <vector>
```

Add after the `LoadSettings` function:

```cpp
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
};

static std::vector<ToastWindow> g_toasts;   // index 0 for active/primary; one per monitor for "all"
static DWORD  g_workerThreadId = 0;
static HANDLE g_workerThread = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
```

- [ ] **Step 2: Add color resolution + the renderer**

Add after the globals from Step 1:

```cpp
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
    const FontFamily& useFf = ff.IsAvailable() ? ff : def;
    Font font(&useFf, (REAL)s.fontSize, style, UnitPixel);

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
            path.AddArc(rc.X, rc.Y, d, d, 180, 90);
            path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270, 90);
            path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0, 90);
            path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90, 90);
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
```

- [ ] **Step 2b: Add monitor selection + present/positioning**

Add after `RenderToast`:

```cpp
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
```

- [ ] **Step 3: Add the window procedure with the fade state machine**

Add after `PresentToast`:

```cpp
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
        RECT wa = WorkAreaForTarget(s.monitor); // best-effort for the timer reposition
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
```

> Note on repeat behavior: while a toast is already visible (`phase == 2`), a new `WM_APP_SHOWTOAST` re-renders, jumps `alpha` to 255, kills the old `HOLD_TIMER`, and (for non-fade) restarts it; for fade mode the fade-in branch immediately sees `alpha >= 255` and resets the hold. This yields the "reuse one toast, reset timer" behavior from the spec.

- [ ] **Step 4: Add module-handle helper, worker thread, and Request/Start/Stop**

Add after `ToastWndProc`:

```cpp
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
```

> Note: `WM_APP_QUIT` is posted as a *thread* message (no window), so the loop checks `msg.message` directly and breaks. We must give the worker a moment to create windows before posting toasts; `Wh_ModInit` (Task 6) starts the worker and the hook only captures real keypresses, so by the time a key is pressed the windows exist.

- [ ] **Step 5: Add a TEMPORARY test trigger** (removed in Task 6)

Replace `Wh_ModInit` with this temporary version that starts the worker and registers Ctrl+Alt+Shift+L to fire a sample toast:

```cpp
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
```

Replace `Wh_ModUninit` with:

```cpp
void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
    if (g_tempHook) { UnhookWindowsHookEx(g_tempHook); g_tempHook = nullptr; }
    StopWorker();
    DeleteCriticalSection(&g_settingsCs);
}
```

- [ ] **Step 6: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0.

- [ ] **Step 7: Manual verification in Windhawk**

1. In Windhawk → Create New Mod → paste the full `lock-keys-notifier.wh.cpp` → Compile. Expected: compiles with no errors.
2. Enable the mod. Press **Ctrl+Alt+Shift+L** a few times.
   - Expected: a toast fades in near bottom-center reading `Caps Lock: ON` / `Caps Lock: OFF`, holds ~1.5s, fades out.
3. In settings, change `positionAnchor`, `backgroundColor` (e.g. `#202020`), `fontSize`, `cornerRadius`, then save and re-trigger. Expected: appearance updates.
4. Disable the mod. Expected: clean unload, no explorer crash.

- [ ] **Step 8: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add worker thread, layered toast window, and rendering"
```

---

### Task 5: Low-level keyboard hook and state detection

Replace the temporary trigger with the real `WH_KEYBOARD_LL` hook installed on the worker thread. Detect key-down edges for the four lock keys, flip the self-tracked ON/OFF state, respect per-key enable settings, and request the toast.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp`

**Interfaces:**
- Consumes: `RequestToast`, `g_settings`, `KeyIndex`.
- Produces: `g_realHook` and `LowLevelKeyboardProc`; the hook is installed inside the worker thread (so its callback runs there, same thread as the windows).

- [ ] **Step 1: Add the key table, tracked-state globals, and the hook proc**

Add after `RequestToast` (above `WorkerThreadProc`):

```cpp
static const int kLockVk[KI_Count] = { VK_CAPITAL, VK_NUMLOCK, VK_SCROLL, VK_INSERT };
static bool g_keyDown[KI_Count]  = { false, false, false, false };
static bool g_keyState[KI_Count] = { false, false, false, false };
static HHOOK g_realHook = nullptr;

static bool KeyEnabled(const Settings& s, int ki) {
    switch (ki) {
        case KI_Caps:   return s.notifyCaps;
        case KI_Num:    return s.notifyNum;
        case KI_Scroll: return s.notifyScroll;
        default:        return s.notifyInsert;
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
        bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool up   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);
        for (int i = 0; i < KI_Count; ++i) {
            if ((int)k->vkCode != kLockVk[i]) continue;
            if (down) {
                if (!g_keyDown[i]) {            // down edge only (ignore auto-repeat)
                    g_keyDown[i] = true;
                    g_keyState[i] = !g_keyState[i];
                    Settings s;
                    EnterCriticalSection(&g_settingsCs);
                    bool enabled = KeyEnabled(g_settings, i);
                    LeaveCriticalSection(&g_settingsCs);
                    if (enabled) RequestToast(i, g_keyState[i]);
                }
            } else if (up) {
                g_keyDown[i] = false;
            }
            break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static void SeedKeyStates() {
    for (int i = 0; i < KI_Count; ++i)
        g_keyState[i] = (GetKeyState(kLockVk[i]) & 1) != 0;
}
```

- [ ] **Step 2: Install/uninstall the real hook inside the worker thread**

In `WorkerThreadProc`, after the window-creation loop (just before `MSG msg;`), add:

```cpp
    SeedKeyStates();
    g_realHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
    if (!g_realHook) Wh_Log(L"keyboard hook install failed");
```

In `WorkerThreadProc`, in the cleanup section (before `DestroyWindow`), add:

```cpp
    if (g_realHook) { UnhookWindowsHookEx(g_realHook); g_realHook = nullptr; }
```

- [ ] **Step 3: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0. (The temporary hook from Task 4 still exists; it is removed in Task 6. Both hooks coexisting compiles fine.)

- [ ] **Step 4: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add low-level keyboard hook and lock-key state detection"
```

---

### Task 6: Lifecycle wiring and cleanup

Remove the temporary trigger from Task 4. `Wh_ModInit` starts the worker (which now owns the real hook); `Wh_ModUninit` stops it; `Wh_ModSettingsChanged` reloads. This is the task that makes the mod do its real job end-to-end.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp`

**Interfaces:**
- Consumes: `StartWorker`, `StopWorker`, `LoadSettings`.

- [ ] **Step 1: Remove the temporary trigger and finalize lifecycle**

Delete the entire `// TEMPORARY (Task 4 verification)` block (the `g_tempHook` global, `TempKbdProc`, and the temporary `Wh_ModInit`/`Wh_ModUninit`). Replace with the final versions:

```cpp
BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    InitializeCriticalSection(&g_settingsCs);
    LoadSettings();
    if (!StartWorker()) {
        Wh_Log(L"worker start failed");
        DeleteCriticalSection(&g_settingsCs);
        return FALSE;
    }
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
    StopWorker();
    DeleteCriticalSection(&g_settingsCs);
}
```

Confirm `Wh_ModSettingsChanged` (from Task 3) remains:

```cpp
void Wh_ModSettingsChanged() {
    Wh_Log(L"Lock Keys Notifier settings changed");
    LoadSettings();
}
```

- [ ] **Step 2: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0.

- [ ] **Step 3: Run helper tests** (regression)

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: `All tests passed`.

- [ ] **Step 4: Full manual verification in Windhawk** (the spec's §10 checklist)

1. Paste the mod into Windhawk, compile, enable.
2. Toggle Caps / Num / Scroll; confirm each shows the correct `... : ON` / `... : OFF`.
3. Enable `notifyInsert`; press Insert; confirm a toast appears.
4. Disable a key (e.g. `notifyScrollLock`); confirm no toast for it.
5. Sweep `positionAnchor` + `offsetX/offsetY`; confirm placement stays inside the work area (above the taskbar).
6. Set `monitor` to `primary` and `all` on a multi-monitor setup; confirm behavior.
7. Change `backgroundColor`, `textColor`, `backgroundOpacity`, `cornerRadius`, `borderColor`/`borderThickness`, `fontFamily`/`fontSize`/`fontBold`/`fontItalic`; confirm rendering.
8. Toggle `fadeEnabled` and change `durationMs`/`fadeDurationMs`.
9. Set `soundMode` to `systemDefault` and `custom` (with a `.wav` path).
10. Edit `textTemplate`, `labelOn`/`labelOff`, and the `name*` settings; confirm substitution.
11. Mash Caps Lock; confirm a single reused toast whose timer resets.
12. Change a setting while the mod runs; confirm it applies on the next toast.
13. Disable the mod; confirm explorer stays stable (no crash, no leaked topmost window).

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Wire up real lifecycle; remove temporary trigger"
```

---

### Task 7: README, mod readme block, and finalize

Write the user-facing documentation and align the in-mod readme block with it.

**Files:**
- Modify: `README.md`
- Modify: `lock-keys-notifier.wh.cpp` (`==WindhawkModReadme==` block)

**Interfaces:** none (documentation only).

- [ ] **Step 1: Write `README.md`**

```markdown
# Lock Keys Notifier

A [Windhawk](https://github.com/ramensoftware/windhawk) mod that displays a
small, customizable toast notification whenever a lock key — **Caps Lock**,
**Num Lock**, **Scroll Lock**, or **Insert** — is toggled, showing its new
**ON/OFF** state.

## Features

- Toast on lock-key toggle, showing the resulting ON/OFF state.
- Per-key enable/disable (ignore the keys you don't care about).
- 9-point positioning with X/Y offsets, on the active, primary, or all monitors.
- Fully themeable: background/text/border colors, opacity, corner radius,
  padding, font family/size/bold/italic, optional per-key accent indicator dot.
- Follows the system light/dark theme and accent color by default; any color is
  overridable with a hex value.
- Optional fade animation and optional sound (system default or a custom WAV).
- Customizable text via a `{key}` / `{state}` template plus editable labels and
  key names.

## How it works

The mod runs inside `explorer.exe` and installs a global low-level keyboard hook
on a dedicated thread. On each lock-key press it tracks the new toggle state and
draws a click-through layered window. The window never steals focus and ignores
mouse input.

## Settings

All options are configured from the Windhawk settings UI. Highlights:

| Setting | Default | Notes |
|---|---|---|
| Notify on Caps/Num/Scroll/Insert | on/on/on/off | Per-key toggles |
| Display duration (ms) | 1500 | Time fully visible before fade-out |
| Target monitor | Active | Active / Primary / All |
| Position + offsets | Bottom center, 0 / 48 | 9-point anchor + px offsets |
| Colors | blank (theme) | Hex; blank follows system theme/accent |
| Font | Segoe UI, 24px | Family, size, bold, italic |
| Text template | `{key}: {state}` | Plus editable ON/OFF labels and key names |
| Sound | None | None / System default / Custom WAV |

## Notes & caveats

- The mod runs in `explorer.exe`. If Explorer is not running, notifications
  pause until it restarts.
- Fullscreen exclusive applications (some games) may cover the topmost toast.
- **Insert** reports the OS toggle bit, not any application's overtype mode,
  which is application-specific. It is off by default.
- Architecture: x86-64 (64-bit Explorer).

## License

[MIT](LICENSE) © 2026 Havrlisan
```

- [ ] **Step 2: Align the mod's `==WindhawkModReadme==` block**

Replace the readme block body with a condensed version (Windhawk renders Markdown here):

```cpp
// ==WindhawkModReadme==
/*
# Lock Keys Notifier

Displays a small, customizable toast whenever a lock key (Caps Lock, Num Lock,
Scroll Lock, or Insert) is toggled, showing its new ON/OFF state.

## Features
- Per-key enable/disable.
- 9-point positioning with offsets, on the active, primary, or all monitors.
- Themeable colors, opacity, corner radius, padding, font, and a per-key accent
  indicator dot. Follows the system light/dark theme and accent by default.
- Optional fade animation and optional sound (system default or custom WAV).
- Customizable text via a {key} / {state} template, plus editable labels and
  key names.

## Notes
- Runs in explorer.exe; notifications pause if Explorer is not running.
- Fullscreen exclusive apps may cover the toast.
- Insert reports the OS toggle bit, not an app's overtype mode (off by default).

License: MIT.
*/
// ==/WindhawkModReadme==
```

- [ ] **Step 3: Run the compile gate** (the readme block is a comment, but confirm nothing broke)

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit code 0.

- [ ] **Step 4: Commit**

```bash
git add README.md lock-keys-notifier.wh.cpp
git commit -m "Add README and align mod readme block"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** keys (Task 5) ✓; state detection incl. Insert caveat (Task 5) ✓; custom layered window render (Task 4) ✓; explorer host + worker thread + LL hook (Tasks 4–6) ✓; single reusable window + reset timer (Task 4) ✓; 9-point anchor + offsets (Task 2 helper, Task 4 present) ✓; monitor active/primary/all (Task 4) ✓; theme follow + accent (Task 3) ✓; all settings (Task 3 block + loader) ✓; fade toggle/duration (Task 4) ✓; sound none/system/custom (Task 4) ✓; indicator dot (Task 4) ✓; per-key accent (Task 4) ✓; text template/labels/names (Task 2 helper + Task 4) ✓; pure helpers tested (Task 2) ✓; README + LICENSE + single-file mod (Tasks 1, 7) ✓; manual verification checklist (Task 6) ✓.
- **Placeholder scan:** no TODO/TBD; every code step shows complete code; every command shows expected output.
- **Type consistency:** `Settings`, `Anchor`, `MonitorTarget`, `SoundMode`, `KeyIndex`, `ToastWindow`, and the helper signatures are used consistently across Tasks 2–6. `RequestToast(int,bool)`, `StartWorker()`, `StopWorker()`, `LoadSettings()` match their call sites.
- **Known execution risks (resolve with the per-step compile gate):** GDI+ API spellings and `UpdateLayeredWindow` premultiplication are the most error-prone; the compile gate plus the Task 4/6 manual checks catch issues. If `clang++` flags resolution of `windhawk_api.h` differs, adjust the `-I` path to `C:\Program Files\Windhawk\Compiler\include` (already included).
