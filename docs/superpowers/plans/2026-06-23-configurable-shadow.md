# Configurable Shadow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the toast's drop shadow configurable — enable/disable, size, opacity, vertical offset, and color — defaulting to the current pixel-identical look.

**Architecture:** Two pure integer helpers (`shadowLayerAlpha`, `shadowMargin`) go in the testable HELPERS section; the GDI+ `DrawShadow` becomes parameterized; `RenderToast` computes the DIB margin from settings and builds the shadow color/alpha. Five new settings are added to the block, `Settings` struct, and `LoadSettings`.

**Tech Stack:** Single-file Windhawk mod (C++23), GDI+, bundled clang. No build system; compile gate + pure-helper unit tests.

## Global Constraints

- Single file `lock-keys-notifier.wh.cpp`, **no local `#include`s** — system headers only.
- Code between `// === HELPERS BEGIN ===` and `// === HELPERS END ===` may depend on **only** `<windows.h>`, `<string>`, `<cstdint>` — no `Wh_*`, no GDI+. Never alter the marker comment text.
- Every settings-block key needs a matching `Wh_Get*Setting(L"<key>")` read with an **identical name**. String settings read via `GetStr` (which frees).
- x86-64 only. Defaults must reproduce today's exact appearance.

---

### Task 1: Pure shadow helpers (`shadowLayerAlpha`, `shadowMargin`)

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` (insert before `// === HELPERS END ===` at line 224)
- Test: `tests/helpers_test.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `int shadowLayerAlpha(int opacity)` — clamps `opacity` to 0–100, returns per-layer alpha `opacity * 25 / 100` (so 40 → 10).
  - `int shadowMargin(bool enabled, int size, int offsetY)` — returns `2` when disabled, else `max(0,size) + abs(offsetY) + 1`.

- [ ] **Step 1: Write the failing tests**

In `tests/helpers_test.cpp`, add before the final `if (g_failures == 0)` block:

```cpp
    // shadowLayerAlpha: 0-100 opacity -> per-layer alpha; default 40 -> 10
    CHECK(shadowLayerAlpha(40) == 10);
    CHECK(shadowLayerAlpha(0) == 0);
    CHECK(shadowLayerAlpha(100) == 25);
    CHECK(shadowLayerAlpha(-5) == 0);     // clamps low
    CHECK(shadowLayerAlpha(150) == 25);   // clamps high

    // shadowMargin: enabled -> size + |offsetY| + 1; defaults 13,4 -> 18 (==old constant)
    CHECK(shadowMargin(true, 13, 4) == 18);
    CHECK(shadowMargin(true, 13, -4) == 18);  // negative offset, same magnitude
    CHECK(shadowMargin(true, 0, 0) == 1);
    CHECK(shadowMargin(false, 13, 4) == 2);   // disabled collapses to AA pad
```

- [ ] **Step 2: Run tests to verify they fail to compile**

```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: FAIL — compile error, `use of undeclared identifier 'shadowLayerAlpha'`.

- [ ] **Step 3: Add the helpers**

In `lock-keys-notifier.wh.cpp`, immediately before the `// === HELPERS END ===` line (currently line 224), insert:

```cpp
// Per-layer shadow alpha (0-255) from a 0-100 opacity. The shadow stacks 14
// layers; opacity 40 -> alpha 10 reproduces the original fixed look.
inline int shadowLayerAlpha(int opacity) {
    if (opacity < 0) opacity = 0;
    if (opacity > 100) opacity = 100;
    return opacity * 25 / 100;
}

// DIB margin (px per side) needed to contain the shadow: spread + drop
// magnitude + 1px AA safety. Collapses to a small pad when disabled.
inline int shadowMargin(bool enabled, int size, int offsetY) {
    if (!enabled) return 2;
    if (size < 0) size = 0;
    int oy = offsetY < 0 ? -offsetY : offsetY;
    return size + oy + 1;
}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: PASS — `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp tests/helpers_test.cpp
git commit -m "Add pure shadow helpers (alpha map, margin)"
```

---

### Task 2: Wire shadow settings through render

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — settings block (~line 105), `Settings` struct (~line 240), `LoadSettings` (~line 326), `DrawShadow` (~line 449), `RenderToast` (~lines 620, 709).

**Interfaces:**
- Consumes: `shadowLayerAlpha`, `shadowMargin` (Task 1); existing `ResolveColor(const std::wstring&, uint32_t) -> uint32_t`.
- Produces: `DrawShadow(Graphics&, const RectF& surface, REAL radius, REAL spread, REAL dy, const Color& color)`.

- [ ] **Step 1: Add the five settings to the `==WindhawkModSettings==` block**

After the `cornerRadius` entry (the two lines `- cornerRadius: 6` / `  $name: Corner radius (px)`), insert:

```
- shadowEnabled: true
  $name: Drop shadow
- shadowSize: 13
  $name: Shadow size (px)
  $description: How far the shadow spreads past the window (0-40).
- shadowOpacity: 40
  $name: Shadow opacity (0-100)
- shadowOffsetY: 4
  $name: Shadow vertical offset (px)
  $description: Downward drop; negative casts the shadow upward.
- shadowColor: ""
  $name: Shadow color
  $description: Hex like #000000. Blank is black. Alpha is ignored; use opacity.
```

- [ ] **Step 2: Add fields to the `Settings` struct**

After the line `int width, height, padding, cornerRadius;` insert:

```cpp
    bool shadowEnabled;
    int shadowSize, shadowOpacity, shadowOffsetY;
    std::wstring shadowColor;
```

- [ ] **Step 3: Read the settings in `LoadSettings`**

After the line `s.cornerRadius = Wh_GetIntSetting(L"cornerRadius");` insert:

```cpp
    s.shadowEnabled = Wh_GetIntSetting(L"shadowEnabled");
    s.shadowSize    = Wh_GetIntSetting(L"shadowSize");
    s.shadowOpacity = Wh_GetIntSetting(L"shadowOpacity");
    s.shadowOffsetY = Wh_GetIntSetting(L"shadowOffsetY");
    s.shadowColor   = GetStr(L"shadowColor");
```

- [ ] **Step 4: Parameterize `DrawShadow`**

Replace the whole function (currently lines 449-463):

```cpp
static void DrawShadow(Graphics& g, const RectF& surface, REAL radius) {
    const int  layers = 14;
    const REAL spread = 13.0f;   // how far the shadow bleeds past the surface
    const REAL dy     = 4.0f;    // downward offset
    for (int i = layers; i >= 1; --i) {
        REAL t = (REAL)i / layers;
        REAL inflate = spread * t;
        RectF sr(surface.X - inflate, surface.Y - inflate + dy,
                 surface.Width + inflate * 2, surface.Height + inflate * 2);
        GraphicsPath sp;
        BuildRoundedRectPath(sp, sr, radius + inflate);
        SolidBrush sb(Color(10, 0, 0, 0));
        g.FillPath(&sb, &sp);
    }
}
```

with:

```cpp
static void DrawShadow(Graphics& g, const RectF& surface, REAL radius,
                       REAL spread, REAL dy, const Color& color) {
    const int layers = 14;
    if (spread <= 0.0f || color.GetA() == 0) return;
    for (int i = layers; i >= 1; --i) {
        REAL t = (REAL)i / layers;
        REAL inflate = spread * t;
        RectF sr(surface.X - inflate, surface.Y - inflate + dy,
                 surface.Width + inflate * 2, surface.Height + inflate * 2);
        GraphicsPath sp;
        BuildRoundedRectPath(sp, sr, radius + inflate);
        SolidBrush sb(color);
        g.FillPath(&sb, &sp);
    }
}
```

- [ ] **Step 5: Compute the margin from settings in `RenderToast`**

Replace the line:

```cpp
    const int margin = 18; // shadow margin on each side (covers spread 13 + dy 4)
```

with:

```cpp
    int margin = shadowMargin(s.shadowEnabled, s.shadowSize, s.shadowOffsetY);
```

- [ ] **Step 6: Build the shadow color and call the parameterized `DrawShadow`**

Replace the line:

```cpp
        DrawShadow(g, surface, radius);
```

with:

```cpp
        if (s.shadowEnabled) {
            uint32_t sc = ResolveColor(s.shadowColor, 0xFF000000u);
            int sa = shadowLayerAlpha(s.shadowOpacity);
            Color shadowCol((BYTE)sa, (BYTE)((sc >> 16) & 0xFF),
                            (BYTE)((sc >> 8) & 0xFF), (BYTE)(sc & 0xFF));
            DrawShadow(g, surface, radius, (REAL)s.shadowSize,
                       (REAL)s.shadowOffsetY, shadowCol);
        }
```

- [ ] **Step 7: Run the whole-mod compile gate**

```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output (clean compile).

- [ ] **Step 8: Re-run helper tests (regression — markers/extractor still valid)**

```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: PASS — `All tests passed`.

- [ ] **Step 9: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Make drop shadow configurable (toggle, size, opacity, offset, color)"
```

---

## Manual verification (post-implementation, per CLAUDE.md)

No automated runtime test exists. After both tasks, load the file in Windhawk (Create New Mod → Compile → Enable) and confirm:
- Defaults look identical to the previous build (size 13, opacity 40, offset 4, black).
- `shadowEnabled` off → no shadow, toast renders flush and stays correctly anchored (no positional shift).
- Increasing `shadowSize`/`shadowOpacity` visibly enlarges/darkens without clipping at the DIB edge.
- Negative `shadowOffsetY` casts the shadow upward without clipping.
- A non-blank `shadowColor` (e.g. `#0040ff`) tints the shadow; alpha byte in the hex is ignored.

## Self-Review

- **Spec coverage:** §4 lists `shadowEnabled`/`shadowSize`/`shadowOpacity`/`shadowOffsetY`/`shadowColor` → Task 2 Step 1 adds all five; §3.4 dynamic margin `ceil(size)+abs(offsetY)+1` → Task 1 `shadowMargin` + Task 2 Step 5; opacity→alpha `×25÷100` → Task 1 `shadowLayerAlpha`; color RGB-only/alpha-ignored → Task 2 Step 6. Covered.
- **Placeholder scan:** none — every code step shows full code.
- **Type consistency:** `DrawShadow` new signature (Step 4) matches its sole call site (Step 6); `shadowLayerAlpha`/`shadowMargin` names identical in Task 1 definition, Task 1 tests, and Task 2 usage.
