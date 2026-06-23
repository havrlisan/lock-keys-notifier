# Font Weight Setting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the boolean `fontBold` ("Bold text") setting with a named **Font weight** dropdown (Thin…Black) defaulting to Semibold.

**Architecture:** Add a pure `parseFontWeight` helper (string → `FW_*` int) inside the testable HELPERS markers; swap the `Settings` field and its read; switch `RenderToast`'s three GDI+ fonts from the `Font(FontFamily*, …, style, …)` constructor to `Font(const LOGFONTW*)` so `lfWeight` takes effect.

**Tech Stack:** C++ (C++23), GDI+, Windhawk mod settings, bundled clang toolchain. Single self-contained file `lock-keys-notifier.wh.cpp` + pure-helper unit tests under `tests/`.

## Global Constraints

- **Single-file constraint:** `lock-keys-notifier.wh.cpp` stays fully self-contained — no local `#include`s, system headers only.
- **HELPERS marker rules:** Code between `// === HELPERS BEGIN ===` and `// === HELPERS END ===` may depend on **only** `<windows.h>`, `<string>`, `<cstdint>`. No `Wh_*`, no GDI+. Never alter the marker comment text.
- **Settings key/read parity:** every key in the `==WindhawkModSettings==` block must have a matching `Wh_Get*Setting(L"<key>")` / `GetStr(L"<key>")` read with the identical name.
- **Architecture:** x86-64 only.
- **Compile gate command (run from repo root, Git Bash):**
  ```bash
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
  ```
- **Unit-test commands (run from repo root, Git Bash):**
  ```bash
  bash tools/extract-helpers.sh
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
  ```
- **Git:** keep commit subjects short; never push.

---

### Task 1: `parseFontWeight` helper + unit test

Add the pure helper and its test first (TDD). The test extracts helpers from the source, so the helper must be added to the source before the test can pass.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` (add helper after `parseInsertMode`, before `parseHexColor` — currently lines 207–209)
- Test: `tests/helpers_test.cpp` (add cases after the `parseInsertMode` block, currently line 29)

**Interfaces:**
- Produces: `int parseFontWeight(const std::wstring& s)` — returns a Windows `FW_*` weight. Mapping: `thin`→`FW_THIN`(100), `light`→`FW_LIGHT`(300), `regular`→`FW_NORMAL`(400), `medium`→`FW_MEDIUM`(500), `semibold`→`FW_SEMIBOLD`(600), `bold`→`FW_BOLD`(700), `black`→`FW_HEAVY`(900). Blank/unknown → `FW_SEMIBOLD`. (`FW_*` are from `<windows.h>`, already permitted in the markers.)

- [ ] **Step 1: Write the failing test**

In `tests/helpers_test.cpp`, immediately after the `parseInsertMode` block (after current line 29), add:

```cpp
    // parseFontWeight
    CHECK(parseFontWeight(L"thin")     == FW_THIN);      // 100
    CHECK(parseFontWeight(L"light")    == FW_LIGHT);     // 300
    CHECK(parseFontWeight(L"regular")  == FW_NORMAL);    // 400
    CHECK(parseFontWeight(L"medium")   == FW_MEDIUM);    // 500
    CHECK(parseFontWeight(L"semibold") == FW_SEMIBOLD);  // 600
    CHECK(parseFontWeight(L"bold")     == FW_BOLD);      // 700
    CHECK(parseFontWeight(L"black")    == FW_HEAVY);     // 900
    CHECK(parseFontWeight(L"")         == FW_SEMIBOLD);  // blank -> default
    CHECK(parseFontWeight(L"bogus")    == FW_SEMIBOLD);  // unknown -> default
```

- [ ] **Step 2: Run the test to verify it fails**

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: compile FAILS with `use of undeclared identifier 'parseFontWeight'` (helper not added yet).

- [ ] **Step 3: Add the helper to the source**

In `lock-keys-notifier.wh.cpp`, after the `parseInsertMode` function (after current line 207, before the blank line and `parseHexColor`), add:

```cpp
inline int parseFontWeight(const std::wstring& s) {
    if (s == L"thin")     return FW_THIN;      // 100
    if (s == L"light")    return FW_LIGHT;     // 300
    if (s == L"regular")  return FW_NORMAL;    // 400
    if (s == L"medium")   return FW_MEDIUM;    // 500
    if (s == L"bold")     return FW_BOLD;      // 700
    if (s == L"black")    return FW_HEAVY;     // 900
    return FW_SEMIBOLD;                        // 600 — default/unknown
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run the same commands as Step 2.
Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp tests/helpers_test.cpp
git commit -m "Add parseFontWeight helper + test"
```

---

### Task 2: Wire the setting into the mod (settings block, struct, LoadSettings, fonts)

Replace the boolean `fontBold` end-to-end with the `fontWeight` dropdown and make `RenderToast` honor the weight. This is one task because the changes are interdependent — the source will not compile cleanly until all four edits are made together (removing `fontBold` from the struct breaks `LoadSettings` and `RenderToast`).

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — settings block (currently lines 140–141), `Settings` struct (line 312), `LoadSettings` (line 406), `RenderToast` font construction (lines 724–733)

**Interfaces:**
- Consumes: `parseFontWeight` from Task 1.
- Produces: `int Settings::fontWeight` replacing `bool fontBold`.

- [ ] **Step 1: Replace the settings-block entry**

In the `==WindhawkModSettings==` block, replace (currently lines 140–141):

```
- fontBold: false
  $name: Bold text
```

with:

```
- fontWeight: semibold
  $name: Font weight
  $options:
  - thin: Thin
  - light: Light
  - regular: Regular
  - medium: Medium
  - semibold: Semibold
  - bold: Bold
  - black: Black
```

- [ ] **Step 2: Update the `Settings` struct**

At line 312, change:

```cpp
    bool fontBold, fontItalic;
```

to:

```cpp
    int fontWeight;
    bool fontItalic;
```

- [ ] **Step 3: Update `LoadSettings`**

At line 406, change:

```cpp
    s.fontBold     = Wh_GetIntSetting(L"fontBold");
```

to:

```cpp
    s.fontWeight   = parseFontWeight(GetStr(L"fontWeight"));
```

- [ ] **Step 4: Switch the three fonts to `LOGFONTW` construction**

In `RenderToast`, replace the font block (currently lines 724–733):

```cpp
    // Fonts (sizes already scaled to the target DPI).
    int style = (s.fontBold ? FontStyleBold : 0) | (s.fontItalic ? FontStyleItalic : 0);
    FontFamily ff(s.fontFamily.c_str());
    FontFamily def(L"Segoe UI");
    const FontFamily* useFf = ff.IsAvailable() ? &ff : &def;
    Font fontName(useFf, (REAL)fontSizeS, style, UnitPixel);
    REAL stateSize = (REAL)fontSizeS * 0.5f; REAL minState = 11.0f * scale;
    if (stateSize < minState) stateSize = minState;
    Font fontState(useFf, stateSize, FontStyleBold, UnitPixel);
    Font fontGlyph(useFf, (REAL)fontSizeS * 0.9f, style, UnitPixel);
```

with:

```cpp
    // Fonts (sizes already scaled to the target DPI). Built via LOGFONTW so an
    // arbitrary lfWeight (Thin..Black) takes effect — GDI+'s FontStyle bitmask
    // only knows Regular/Bold.
    FontFamily ff(s.fontFamily.c_str());
    const wchar_t* face = ff.IsAvailable() ? s.fontFamily.c_str() : L"Segoe UI";

    REAL stateSize = (REAL)fontSizeS * 0.5f; REAL minState = 11.0f * scale;
    if (stateSize < minState) stateSize = minState;

    // emPx = em height in device pixels; weight = lfWeight; the state label is
    // never lighter than the body (emphasis), so it floors at FW_BOLD.
    auto makeFont = [&](REAL emPx, int weight) {
        LOGFONTW lf{};
        lf.lfHeight  = -(LONG)(emPx + 0.5f);
        lf.lfWeight  = weight;
        lf.lfItalic  = s.fontItalic ? TRUE : FALSE;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcsncpy_s(lf.lfFaceName, LF_FACESIZE, face, _TRUNCATE);
        return Font(&lf);
    };

    // State label floors at FW_BOLD (the spec's max(weight, FW_BOLD) rule) so it
    // stays an emphasis element, never lighter than the body.
    int stateWeight = s.fontWeight > FW_BOLD ? s.fontWeight : FW_BOLD;
    Font fontName  = makeFont((REAL)fontSizeS,        s.fontWeight);
    Font fontState = makeFont(stateSize,              stateWeight);
    Font fontGlyph = makeFont((REAL)fontSizeS * 0.9f, s.fontWeight);
```

(`Font` is copyable in GDI+, so returning it from the lambda and copy-initializing is fine. Note: the italic flag now applies to the state label too, where previously it did not — acceptable and more consistent; confirm during manual verification.)

- [ ] **Step 5: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output (exit 0). If `wcsncpy_s` is unavailable in this toolchain, fall back to `lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);`.

- [ ] **Step 6: Re-run the unit tests (guard against helper-marker breakage)**

Run:
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: `All tests passed`.

- [ ] **Step 7: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Replace Bold text with Font weight setting"
```

---

### Task 3: Docs

Update prose that references the old "Bold text" setting.

**Files:**
- Modify: any README and the design spec that mention "Bold text" / `fontBold` (grep to find them).

- [ ] **Step 1: Find references**

Run:
```bash
grep -rn -i "bold text\|fontBold" --include=*.md --include=*.wh.cpp .
```
Expected: the `==WindhawkModReadme==` section inside `lock-keys-notifier.wh.cpp` (if it lists settings) and/or `README.md` / docs. (The `.wh.cpp` settings/struct/code references were already handled in Tasks 1–2; here we only touch prose/readme text.)

- [ ] **Step 2: Update each reference**

For each prose hit, replace the "Bold text" description with the Font weight dropdown: "**Font weight** — Thin / Light / Regular / Medium / Semibold / Bold / Black (default Semibold)." Add a one-line migration note where the settings are described: "Replaces the former *Bold text* toggle; existing installs reset to the Semibold default."

- [ ] **Step 3: If `lock-keys-notifier.wh.cpp` was edited, re-run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output (exit 0). (Skip if only `.md` files changed.)

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "Document Font weight setting"
```

---

## Manual verification (post-implementation, in Windhawk)

Not automatable — the mod only runs injected into `explorer.exe`:

1. Load the file in Windhawk (Create New Mod → Compile → Enable).
2. Confirm a fresh enable defaults to Semibold and reads noticeably heavier than the old Regular.
3. Cycle the dropdown through each weight; confirm the body text weight changes and the state label ("ON"/"OFF") stays at least bold and never lighter than the body.
4. Confirm the toast still sizes and positions correctly (watch for the LOGFONT size shift noted in the spec).
5. Toggle Italic; confirm italic still applies to the body (and now also the state label).
