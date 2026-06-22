# Fluent Toast Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redraw the lock-keys toast in a Windows 11 Fluent style — soft drop shadow, translucent surface, rounded corners, accent-colored state — with three user-selectable layouts (Pill, Tile, Minimal).

**Architecture:** All changes live in the single file `lock-keys-notifier.wh.cpp` (Windhawk compiles one self-contained file — no local `#include`s). The rendering rewrite stays inside `RenderToast`, factored into a shared surface/shadow pass plus per-layout content helpers that take a `ToastCtx` struct. A new pure-helper (`parseLayout`) goes in the testable seam between the `=== HELPERS BEGIN/END ===` markers. Worker-thread/hook lifecycle, key-up detection, monitors, fade, and sound are untouched.

**Tech Stack:** C++23, Win32 + GDI+ (`gdiplus`), the bundled Windhawk LLVM/clang toolchain. No build system; correctness gate is clang `-fsyntax-only` over the whole file plus a small unit-test exe over the extracted helpers.

## Global Constraints

- **Single self-contained file:** `lock-keys-notifier.wh.cpp` — no local `#include`s, system headers only. Never split it.
- **Helper-seam purity:** code between `// === HELPERS BEGIN ===` and `// === HELPERS END ===` may depend on **only** `<windows.h>`, `<string>`, `<cstdint>`. No `Wh_*`, no GDI+. Never alter the marker comment text (the extractor matches them literally).
- **Architecture x86-64 only.** `Insert` reports the OS toggle bit; off by default.
- **Settings parity rule:** every key in the `==WindhawkModSettings==` block must have a matching `Wh_Get*Setting(L"<key>")` read with an identical name.
- **Commits:** short subjects; never push without being asked.

### Compile gate (whole-mod, syntax-only — the primary per-task check)

```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit 0.

### Pure-helper unit tests

```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: `All tests passed`.

> **Testing reality:** there is no automated runtime test — the mod only runs injected into `explorer.exe`. For Tasks 2–4, the per-task gate is the compile gate + unit tests; visual/runtime behavior is verified by the **manual checklist** (spec §7), performed by the human reviewer after the branch is built. Each rendering task lists the manual checks it introduces; do not fabricate runtime asserts.

---

### Task 1: Add `parseLayout` pure helper (additive)

Purely additive: adds the layout enum + parser to the test seam and a unit test. The whole mod still compiles (nothing removed yet).

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` (helper seam, ~line 159)
- Test: `tests/helpers_test.cpp`

**Interfaces:**
- Produces: `enum class ToastLayout { Pill, Tile, Minimal };` and `inline ToastLayout parseLayout(const std::wstring& s)` — returns `Tile` for `L"tile"`, `Minimal` for `L"minimal"`, else `Pill`. Consumed by `Settings`/`LoadSettings` in Task 2.

- [ ] **Step 1: Write the failing test** — add to `tests/helpers_test.cpp`, immediately after the `formatTemplate` block (after line 21):

```cpp
    // parseLayout
    CHECK(parseLayout(L"pill") == ToastLayout::Pill);
    CHECK(parseLayout(L"tile") == ToastLayout::Tile);
    CHECK(parseLayout(L"minimal") == ToastLayout::Minimal);
    CHECK(parseLayout(L"") == ToastLayout::Pill);        // blank -> default
    CHECK(parseLayout(L"bogus") == ToastLayout::Pill);   // unknown -> default
```

- [ ] **Step 2: Run the unit test build to verify it fails**

Run the Pure-helper unit-test commands from Global Constraints.
Expected: FAIL — compile error, `parseLayout`/`ToastLayout` not declared.

- [ ] **Step 3: Add the helper to the seam** — in `lock-keys-notifier.wh.cpp`, directly after the `Anchor` enum (after line 163, before `parseHexColor`):

```cpp
enum class ToastLayout { Pill, Tile, Minimal };

inline ToastLayout parseLayout(const std::wstring& s) {
    if (s == L"tile")    return ToastLayout::Tile;
    if (s == L"minimal") return ToastLayout::Minimal;
    return ToastLayout::Pill;
}
```

- [ ] **Step 4: Run the unit tests to verify they pass**

Run the Pure-helper unit-test commands.
Expected: `All tests passed`.

- [ ] **Step 5: Run the whole-mod compile gate** (confirm additive change didn't break the mod)

Run the Compile gate command. Expected: exit 0, no output.

- [ ] **Step 6: Commit**

```bash
git add lock-keys-notifier.wh.cpp tests/helpers_test.cpp
git commit -m "Add ToastLayout enum + parseLayout helper"
```

---

### Task 2: Fluent surface, drop shadow, and Pill layout

The core rewrite. Adds the settings, repurposes the struct, rewrites `RenderToast` into a shared surface/shadow pass + a `ToastCtx`-based Pill renderer, expands the layered window for the shadow, and drops the now-dead `formatTemplate`/`textTemplate`/`showIndicator`. After this task, **all** `layout` values render as Pill (Tile/Minimal specialize in Tasks 3–4); the setting is functional throughout.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — settings block (~42–147), `Settings` struct (~231–254), `LoadSettings` (~308–353), helper seam (remove `formatTemplate`), new helpers + `RenderToast` (~407–534), `ToastWindow` (~364–372), `PresentToast` (~562–582)
- Modify: `tests/helpers_test.cpp` (remove `formatTemplate` checks)

**Interfaces:**
- Consumes: `parseLayout`, `ToastLayout` (Task 1); existing `KeyName`, `KeyAccent`, `ResolveColor`, `SystemUsesLightTheme`, `SystemAccentArgb`, `computeToastRect`.
- Produces:
  - `Settings.layout` (`ToastLayout`), `Settings.showIcon` (`bool`); removes `Settings.showIndicator`, `Settings.textTemplate`.
  - `ToastWindow.margin` (`int`) — shadow margin baked into the DIB.
  - `static const wchar_t* KeyGlyph(int ki)`.
  - `static Color ToGdiColor(uint32_t argb)`; `static void BuildRoundedRectPath(GraphicsPath&, const RectF&, REAL radius)`; `struct PillColors { Color fill, border, text; }`; `static PillColors MakePillColors(uint32_t baseAccent, bool isOn, bool light)`; `static void DrawShadow(Graphics&, const RectF& surface, REAL radius)`.
  - `struct ToastCtx { bool light, isOn; uint32_t fg, acc; std::wstring name, state; const wchar_t* glyph; bool showGlyph; int fontSize, padding; const Gdiplus::Font *fontName, *fontState, *fontGlyph; };`
  - `static SIZE MeasurePill(Graphics&, const ToastCtx&)` and `static void DrawPill(Graphics&, const ToastCtx&, const RectF& surface)` — consumed by the Task 3/4 dispatch.

- [ ] **Step 1: Update the settings block** — in the `==WindhawkModSettings==` block: add `layout` and `showIcon`, remove `showIndicator` and `textTemplate`, bump `cornerRadius` default to 10.

  Replace the `cornerRadius` line (line 98–99):
```
- cornerRadius: 10
  $name: Corner radius (px)
```
  Add, immediately after the `notifyInsert` block (after line 52), the layout selector:
```
- layout: pill
  $name: Layout
  $options:
  - pill: Pill — name + state pill
  - tile: Tile — icon tile + two lines
  - minimal: Minimal — glyph + colored state word
```
  Replace the `showIndicator` block (lines 121–122) with:
```
- showIcon: false
  $name: Show key icon glyph
```
  Delete the `textTemplate` block (lines 132–134).

- [ ] **Step 2: Update the in-file ReadMe block** — in `==WindhawkModReadme==`: in the Features list replace the "per-key accent indicator dot" line and the "{key} / {state} template" line:
```
- Three layouts (Pill, Tile, Minimal), themeable colors, opacity, corner radius,
  padding, font, soft drop shadow, and an optional per-key accent state. Follows
  the system light/dark theme and accent by default.
- Optional fade animation and optional sound (system default or custom WAV).
- Editable ON/OFF labels and per-key display names; optional key icon glyph.
```

- [ ] **Step 3: Update the `Settings` struct** — replace the `bool showIndicator;` member and the `std::wstring textTemplate, labelOn, labelOff;` member:
```cpp
    bool showIcon;
    ToastLayout layout;
```
  and
```cpp
    std::wstring labelOn, labelOff;
```
  (Place `showIcon`/`layout` wherever readable, e.g. replacing the old `showIndicator` line.)

- [ ] **Step 4: Update `LoadSettings`** — remove the `showIndicator` and `textTemplate` reads; add:
```cpp
    s.layout   = parseLayout(GetStr(L"layout"));
    s.showIcon = Wh_GetIntSetting(L"showIcon");
```
  (Delete `s.showIndicator = Wh_GetIntSetting(L"showIndicator");` and `s.textTemplate = GetStr(L"textTemplate");`.)

- [ ] **Step 5: Remove the dead `formatTemplate` helper and its test**

  In `lock-keys-notifier.wh.cpp`, delete the entire `inline std::wstring formatTemplate(...) { ... }` function from the seam (currently ~194–205).
  In `tests/helpers_test.cpp`, delete the three `formatTemplate` `CHECK` lines (18–21) and the `// formatTemplate` comment.

- [ ] **Step 6: Add `margin` to `ToastWindow`** — add a member:
```cpp
    int  margin = 0;         // shadow margin baked into the DIB on each side
```

- [ ] **Step 7: Add `KeyGlyph`** — next to `KeyAccent` (after line 404):
```cpp
static const wchar_t* KeyGlyph(int ki) {
    switch (ki) {
        case KI_Caps:   return L"⇪"; // caps lock symbol
        case KI_Num:    return L"#";
        case KI_Scroll: return L"⤓"; // downwards arrow to bar
        default:        return L"⎀"; // insertion symbol
    }
}
```

- [ ] **Step 8: Add the shared GDI+ drawing helpers** — directly above `RenderToast` (before line 407), after `using namespace Gdiplus;` is already in effect:
```cpp
static Color ToGdiColor(uint32_t a) {
    return Color((a >> 24) & 0xFF, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF);
}

static void BuildRoundedRectPath(GraphicsPath& path, const RectF& rc, REAL radius) {
    if (radius <= 0) { path.AddRectangle(rc); return; }
    REAL d = radius * 2;
    path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

struct PillColors { Color fill; Color border; Color text; };

// ON: a translucent tint of the accent with a legible (lightened-on-dark) text.
// OFF: a calm neutral gray, theme-aware. Never red.
static PillColors MakePillColors(uint32_t baseAccent, bool isOn, bool light) {
    BYTE r = (baseAccent >> 16) & 0xFF, g = (baseAccent >> 8) & 0xFF, b = baseAccent & 0xFF;
    if (isOn) {
        BYTE tr = light ? r : (BYTE)(r + (255 - r) * 40 / 100);
        BYTE tg = light ? g : (BYTE)(g + (255 - g) * 40 / 100);
        BYTE tb = light ? b : (BYTE)(b + (255 - b) * 40 / 100);
        return { Color(46, r, g, b), Color(110, r, g, b), Color(255, tr, tg, tb) };
    }
    if (light) return { Color(14, 0, 0, 0),       Color(34, 0, 0, 0),       Color(170, 70, 70, 70) };
    return       { Color(16, 255, 255, 255), Color(30, 255, 255, 255), Color(200, 170, 170, 170) };
}

// Soft drop shadow: stack translucent rounded rects, densest near the surface,
// fading outward. The surface (drawn opaque afterward) covers the inner buildup,
// so only the protruding ring shows.
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

struct ToastCtx {
    bool light, isOn;
    uint32_t fg;          // opaque name/glyph text color
    uint32_t acc;         // ON base accent color
    std::wstring name, state;
    const wchar_t* glyph;
    bool showGlyph;
    int fontSize, padding;
    const Font* fontName;
    const Font* fontState;
    const Font* fontGlyph;
};

// Returns the surface size (content + padding) for the Pill layout.
static SIZE MeasurePill(Graphics& g, const ToastCtx& c) {
    const REAL gap = 8.0f, pillPadX = 11.0f, pillPadY = 4.0f;
    REAL glyphW = 0, glyphH = 0;
    if (c.showGlyph) {
        RectF b; g.MeasureString(c.glyph, -1, c.fontGlyph, PointF(0, 0), &b);
        glyphW = b.Width; glyphH = b.Height;
    }
    RectF nb; g.MeasureString(c.name.c_str(),  -1, c.fontName,  PointF(0, 0), &nb);
    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontState, PointF(0, 0), &sb);
    REAL pillW = sb.Width + pillPadX * 2, pillH = sb.Height + pillPadY * 2;
    REAL contentW = (c.showGlyph ? glyphW + gap : 0) + nb.Width + gap + pillW;
    REAL contentH = max(nb.Height, max(glyphH, pillH));
    return SIZE{ (int)(contentW + c.padding * 2 + 0.5f),
                 (int)(contentH + c.padding * 2 + 0.5f) };
}

static void DrawPill(Graphics& g, const ToastCtx& c, const RectF& surface) {
    const REAL gap = 8.0f, pillPadX = 11.0f, pillPadY = 4.0f;
    REAL x = surface.X + c.padding;
    REAL cy = surface.Y + surface.Height / 2;
    StringFormat leftFmt; leftFmt.SetAlignment(StringAlignmentNear); leftFmt.SetLineAlignment(StringAlignmentCenter);

    if (c.showGlyph) {
        RectF gb; g.MeasureString(c.glyph, -1, c.fontGlyph, PointF(0, 0), &gb);
        RectF lay(x, surface.Y, gb.Width + 1, surface.Height);
        SolidBrush br(ToGdiColor(c.fg));
        g.DrawString(c.glyph, -1, c.fontGlyph, lay, &leftFmt, &br);
        x += gb.Width + gap;
    }

    RectF nb; g.MeasureString(c.name.c_str(), -1, c.fontName, PointF(0, 0), &nb);
    {
        RectF lay(x, surface.Y, nb.Width + 1, surface.Height);
        SolidBrush br(ToGdiColor(c.fg));
        g.DrawString(c.name.c_str(), -1, c.fontName, lay, &leftFmt, &br);
        x += nb.Width + gap;
    }

    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontState, PointF(0, 0), &sb);
    REAL pillW = sb.Width + pillPadX * 2, pillH = sb.Height + pillPadY * 2;
    RectF pill(x, cy - pillH / 2, pillW, pillH);
    PillColors pc = MakePillColors(c.acc, c.isOn, c.light);
    GraphicsPath pp; BuildRoundedRectPath(pp, pill, pillH / 2);
    SolidBrush pf(pc.fill); g.FillPath(&pf, &pp);
    Pen pe(pc.border, 1.0f);  g.DrawPath(&pe, &pp);
    StringFormat center; center.SetAlignment(StringAlignmentCenter); center.SetLineAlignment(StringAlignmentCenter);
    SolidBrush pt(pc.text); g.DrawString(c.state.c_str(), -1, c.fontState, pill, &center, &pt);
}
```

- [ ] **Step 9: Rewrite `RenderToast`** — replace the whole function body (lines 407–534) with:
```cpp
static bool RenderToast(ToastWindow& tw, const Settings& s, int keyIndex, bool isOn) {
    int fontSize = s.fontSize < 1 ? 1 : s.fontSize;
    int padding  = s.padding  < 0 ? 0 : s.padding;
    int cornerRadius = s.cornerRadius < 0 ? 0 : s.cornerRadius;
    const int margin = 16; // shadow margin on each side

    bool light = SystemUsesLightTheme();
    uint32_t themeBg   = light ? 0xFFFFFFFFu : 0xFF202020u;
    uint32_t themeText = light ? 0xFF000000u : 0xFFFFFFFFu;
    uint32_t accent    = SystemAccentArgb();

    uint32_t bg  = ResolveColor(s.backgroundColor, themeBg);
    uint32_t fg  = 0xFF000000u | (ResolveColor(s.textColor, themeText) & 0x00FFFFFFu);
    uint32_t acc = ResolveColor(KeyAccent(s, keyIndex), accent);

    int op = s.backgroundOpacity < 0 ? 0 : s.backgroundOpacity > 100 ? 100 : s.backgroundOpacity;
    int bgA = ((bg >> 24) & 0xFF) * op / 100;
    bg = (uint32_t(bgA) << 24) | (bg & 0x00FFFFFFu);

    bool hasBorder = s.borderThickness > 0;
    REAL borderW   = hasBorder ? (REAL)s.borderThickness : 1.0f;
    uint32_t borderCol = hasBorder ? ResolveColor(s.borderColor, accent)
                                   : (light ? 0x14000000u : 0x20FFFFFFu);

    // Fonts.
    int style = (s.fontBold ? FontStyleBold : 0) | (s.fontItalic ? FontStyleItalic : 0);
    FontFamily ff(s.fontFamily.c_str());
    FontFamily def(L"Segoe UI");
    const FontFamily* useFf = ff.IsAvailable() ? &ff : &def;
    Font fontName(useFf, (REAL)fontSize, style, UnitPixel);
    REAL stateSize = (REAL)fontSize * 0.5f; if (stateSize < 11.0f) stateSize = 11.0f;
    Font fontState(useFf, stateSize, FontStyleBold, UnitPixel);
    Font fontGlyph(useFf, (REAL)fontSize * 0.9f, style, UnitPixel);

    ToastCtx c{};
    c.light = light; c.isOn = isOn; c.fg = fg; c.acc = acc;
    c.name = KeyName(s, keyIndex);
    c.state = isOn ? s.labelOn : s.labelOff;
    c.glyph = KeyGlyph(keyIndex);
    c.showGlyph = s.showIcon;
    c.fontSize = fontSize; c.padding = padding;
    c.fontName = &fontName; c.fontState = &fontState; c.fontGlyph = &fontGlyph;

    // Measure surface size (Tile/Minimal specialize in later tasks; Pill is the default).
    SIZE surf;
    {
        HDC screen = GetDC(nullptr);
        Graphics g(screen);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        if (s.autoSize) surf = MeasurePill(g, c);
        else            surf = SIZE{ s.width, s.height };
        ReleaseDC(nullptr, screen);
    }
    if (surf.cx < 1) surf.cx = 1;
    if (surf.cy < 1) surf.cy = 1;

    int dibW = surf.cx + margin * 2;
    int dibH = surf.cy + margin * 2;

    if (tw.size.cx != dibW || tw.size.cy != dibH || !tw.dib) {
        if (tw.dib) { DeleteObject(tw.dib); tw.dib = nullptr; tw.bits = nullptr; }
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = dibW;
        bmi.bmiHeader.biHeight = -dibH;     // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        tw.dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &tw.bits, nullptr, 0);
        if (!tw.dib) return false;
        tw.size = SIZE{ dibW, dibH };
    }
    tw.margin = margin;

    HDC memDC = CreateCompatibleDC(nullptr);
    HGDIOBJ oldBmp = SelectObject(memDC, tw.dib);
    {
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.Clear(Color(0, 0, 0, 0));

        RectF surface((REAL)margin + 0.5f, (REAL)margin + 0.5f,
                      (REAL)surf.cx - 1.0f, (REAL)surf.cy - 1.0f);
        REAL radius = (REAL)cornerRadius;

        DrawShadow(g, surface, radius);

        GraphicsPath path;
        BuildRoundedRectPath(path, surface, radius);
        SolidBrush bgBrush(ToGdiColor(bg));
        g.FillPath(&bgBrush, &path);
        Pen borderPen(ToGdiColor(borderCol), borderW);
        g.DrawPath(&borderPen, &path);

        DrawPill(g, c, surface);
    }
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);

    // Premultiply alpha for UpdateLayeredWindow.
    uint8_t* px = (uint8_t*)tw.bits;
    for (int i = 0; i < dibW * dibH; ++i) {
        uint8_t a = px[i * 4 + 3];
        px[i * 4 + 0] = (uint8_t)(px[i * 4 + 0] * a / 255);
        px[i * 4 + 1] = (uint8_t)(px[i * 4 + 1] * a / 255);
        px[i * 4 + 2] = (uint8_t)(px[i * 4 + 2] * a / 255);
    }
    return true;
}
```

- [ ] **Step 10: Offset `PresentToast` by the shadow margin** — replace the first three lines of `PresentToast` (lines 563–565):
```cpp
    SIZE surfaceSize{ tw.size.cx - tw.margin * 2, tw.size.cy - tw.margin * 2 };
    RECT r = computeToastRect(s.anchor, surfaceSize, s.offsetX, s.offsetY, workArea);
    POINT ptPos{ r.left - tw.margin, r.top - tw.margin };
    SIZE szWnd{ tw.size.cx, tw.size.cy };
    POINT ptSrc{ 0, 0 };
```
  (The anchored position is computed from the **visible surface** size, then the window origin is shifted out by the margin so the surface lands on the anchor and the shadow bleeds outward.)

- [ ] **Step 11: Run the whole-mod compile gate**

Run the Compile gate command. Expected: exit 0, no output. If `max` is unresolved, confirm `<algorithm>`/GDI+ `max` macro is available — GDI+ headers pull in the `max` macro via `windows.h`; if a conflict appears, qualify as `(a < b ? b : a)` inline. (Expected: compiles as written; GDI+ relies on the `max` macro.)

- [ ] **Step 12: Run the pure-helper unit tests**

Run the Pure-helper unit-test commands. Expected: `All tests passed`.

- [ ] **Step 13: Commit**

```bash
git add lock-keys-notifier.wh.cpp tests/helpers_test.cpp
git commit -m "Redesign toast: Fluent surface, drop shadow, Pill layout"
```

**Manual checks introduced (for later human verification):** Pill layout renders with soft shadow and rounded surface; ON shows accent pill, OFF neutral gray; `showIcon` on adds the glyph; position stays anchored (shadow margin doesn't shift it); light + dark theme; multi-monitor "all".

---

### Task 3: Tile layout

Adds the Tile renderer (rounded icon tile + two stacked lines) and dispatches to it.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` (add `MeasureTile`/`DrawTile`; dispatch in `RenderToast`)

**Interfaces:**
- Consumes: `ToastCtx`, `ToGdiColor`, `BuildRoundedRectPath`, `MakePillColors`, `KeyGlyph` (Task 2).
- Produces: `static SIZE MeasureTile(Graphics&, const ToastCtx&)`, `static void DrawTile(Graphics&, const ToastCtx&, const RectF& surface)`.

- [ ] **Step 1: Add the Tile helpers** — directly after `DrawPill` (from Task 2):
```cpp
// Tile always shows its icon tile (the defining element), regardless of showIcon.
static SIZE MeasureTile(Graphics& g, const ToastCtx& c) {
    const REAL gap = 12.0f, lineGap = 2.0f;
    REAL tile = (REAL)c.fontSize * 1.6f;
    RectF nb; g.MeasureString(c.name.c_str(),  -1, c.fontName,  PointF(0, 0), &nb);
    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontState, PointF(0, 0), &sb);
    REAL textW = max(nb.Width, sb.Width);
    REAL textH = nb.Height + lineGap + sb.Height;
    REAL contentW = tile + gap + textW;
    REAL contentH = max(tile, textH);
    return SIZE{ (int)(contentW + c.padding * 2 + 0.5f),
                 (int)(contentH + c.padding * 2 + 0.5f) };
}

static void DrawTile(Graphics& g, const ToastCtx& c, const RectF& surface) {
    const REAL gap = 12.0f, lineGap = 2.0f;
    REAL tile = (REAL)c.fontSize * 1.6f;
    PillColors tc = MakePillColors(c.acc, c.isOn, c.light);

    RectF tileRc(surface.X + c.padding,
                 surface.Y + (surface.Height - tile) / 2, tile, tile);
    GraphicsPath tp; BuildRoundedRectPath(tp, tileRc, tile * 0.22f);
    SolidBrush tf(tc.fill); g.FillPath(&tf, &tp);
    Pen te(tc.border, 1.0f); g.DrawPath(&te, &tp);

    StringFormat center; center.SetAlignment(StringAlignmentCenter); center.SetLineAlignment(StringAlignmentCenter);
    SolidBrush gbr(c.isOn ? tc.text : ToGdiColor(c.fg));
    g.DrawString(c.glyph, -1, c.fontGlyph, tileRc, &center, &gbr);

    RectF nb; g.MeasureString(c.name.c_str(),  -1, c.fontName,  PointF(0, 0), &nb);
    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontState, PointF(0, 0), &sb);
    REAL textH = nb.Height + lineGap + sb.Height;
    REAL tx = tileRc.X + tile + gap;
    REAL ty = surface.Y + (surface.Height - textH) / 2;
    StringFormat leftFmt; leftFmt.SetAlignment(StringAlignmentNear); leftFmt.SetLineAlignment(StringAlignmentNear);

    SolidBrush nbr(ToGdiColor(c.fg));
    g.DrawString(c.name.c_str(), -1, c.fontName, RectF(tx, ty, nb.Width + 2, nb.Height + 2), &leftFmt, &nbr);
    SolidBrush sbr(tc.text);
    g.DrawString(c.state.c_str(), -1, c.fontState,
                 RectF(tx, ty + nb.Height + lineGap, sb.Width + 2, sb.Height + 2), &leftFmt, &sbr);
}
```

- [ ] **Step 2: Dispatch to Tile in measurement** — in `RenderToast`, replace the measurement `if (s.autoSize)` line:
```cpp
        if (s.autoSize) {
            surf = (s.layout == ToastLayout::Tile) ? MeasureTile(g, c) : MeasurePill(g, c);
        } else {
            surf = SIZE{ s.width, s.height };
        }
```

- [ ] **Step 3: Dispatch to Tile in drawing** — in `RenderToast`, replace the `DrawPill(g, c, surface);` line:
```cpp
        if (s.layout == ToastLayout::Tile) DrawTile(g, c, surface);
        else                               DrawPill(g, c, surface);
```

- [ ] **Step 4: Run the whole-mod compile gate**

Run the Compile gate command. Expected: exit 0, no output.

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add Tile layout"
```

**Manual checks introduced:** `layout = tile` renders icon tile + two lines; tile tints accent when ON, neutral when OFF; tile shows its glyph even with `showIcon` off.

---

### Task 4: Minimal layout

Adds the Minimal renderer (leading glyph + name + colored state word, single line) and dispatches to it.

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` (add `MeasureMinimal`/`DrawMinimal`; dispatch in `RenderToast`)

**Interfaces:**
- Consumes: `ToastCtx`, `ToGdiColor`, `MakePillColors` (Task 2).
- Produces: `static SIZE MeasureMinimal(Graphics&, const ToastCtx&)`, `static void DrawMinimal(Graphics&, const ToastCtx&, const RectF& surface)`.

- [ ] **Step 1: Add the Minimal helpers** — directly after `DrawTile` (from Task 3):
```cpp
// Minimal: glyph (if enabled) + name + state word, all one line. State is shown by
// color (accent when ON, neutral when OFF) at the name's font size — no pill.
static SIZE MeasureMinimal(Graphics& g, const ToastCtx& c) {
    const REAL gap = 8.0f;
    REAL glyphW = 0, glyphH = 0;
    if (c.showGlyph) {
        RectF b; g.MeasureString(c.glyph, -1, c.fontGlyph, PointF(0, 0), &b);
        glyphW = b.Width; glyphH = b.Height;
    }
    RectF nb; g.MeasureString(c.name.c_str(),  -1, c.fontName, PointF(0, 0), &nb);
    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontName, PointF(0, 0), &sb);
    REAL contentW = (c.showGlyph ? glyphW + gap : 0) + nb.Width + gap + sb.Width;
    REAL contentH = max(nb.Height, max(glyphH, sb.Height));
    return SIZE{ (int)(contentW + c.padding * 2 + 0.5f),
                 (int)(contentH + c.padding * 2 + 0.5f) };
}

static void DrawMinimal(Graphics& g, const ToastCtx& c, const RectF& surface) {
    const REAL gap = 8.0f;
    REAL x = surface.X + c.padding;
    StringFormat leftFmt; leftFmt.SetAlignment(StringAlignmentNear); leftFmt.SetLineAlignment(StringAlignmentCenter);

    if (c.showGlyph) {
        RectF gb; g.MeasureString(c.glyph, -1, c.fontGlyph, PointF(0, 0), &gb);
        SolidBrush br(ToGdiColor(c.fg));
        g.DrawString(c.glyph, -1, c.fontGlyph, RectF(x, surface.Y, gb.Width + 1, surface.Height), &leftFmt, &br);
        x += gb.Width + gap;
    }

    RectF nb; g.MeasureString(c.name.c_str(), -1, c.fontName, PointF(0, 0), &nb);
    {
        SolidBrush br(ToGdiColor(c.fg));
        g.DrawString(c.name.c_str(), -1, c.fontName, RectF(x, surface.Y, nb.Width + 1, surface.Height), &leftFmt, &br);
        x += nb.Width + gap;
    }

    RectF sb; g.MeasureString(c.state.c_str(), -1, c.fontName, PointF(0, 0), &sb);
    PillColors pc = MakePillColors(c.acc, c.isOn, c.light);
    SolidBrush sbr(pc.text);
    g.DrawString(c.state.c_str(), -1, c.fontName, RectF(x, surface.Y, sb.Width + 1, surface.Height), &leftFmt, &sbr);
}
```

- [ ] **Step 2: Dispatch to Minimal in measurement** — in `RenderToast`, update the autosize dispatch to a switch:
```cpp
        if (s.autoSize) {
            switch (s.layout) {
                case ToastLayout::Tile:    surf = MeasureTile(g, c);    break;
                case ToastLayout::Minimal: surf = MeasureMinimal(g, c); break;
                default:                   surf = MeasurePill(g, c);    break;
            }
        } else {
            surf = SIZE{ s.width, s.height };
        }
```

- [ ] **Step 3: Dispatch to Minimal in drawing** — in `RenderToast`, update the draw dispatch:
```cpp
        switch (s.layout) {
            case ToastLayout::Tile:    DrawTile(g, c, surface);    break;
            case ToastLayout::Minimal: DrawMinimal(g, c, surface); break;
            default:                   DrawPill(g, c, surface);    break;
        }
```

- [ ] **Step 4: Run the whole-mod compile gate**

Run the Compile gate command. Expected: exit 0, no output.

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add Minimal layout"
```

**Manual checks introduced:** `layout = minimal` renders glyph (when enabled) + name + colored state word on one line; state word is accent-colored ON, neutral OFF.

---

### Task 5: External docs + final verification

Aligns the external docs with the new settings and runs the full gate. No behavior change.

**Files:**
- Modify: `README.md`, `CLAUDE.md`

- [ ] **Step 1: Update `README.md`** — replace the indicator-dot feature line (line 14):
```
  padding, font family/size/bold/italic, soft drop shadow, and an optional per-key accent state pill.
```
  Replace the text-template feature line (line 18):
```
- Three layouts (Pill, Tile, Minimal); editable ON/OFF labels and key names,
```
  In the settings table, replace the `Text template` row (line 40) with:
```
| Layout | `Pill` | Pill / Tile / Minimal; ON uses the system accent, OFF neutral |
```

- [ ] **Step 2: Update `CLAUDE.md`** — in the helper-seam description (line 17), replace `formatTemplate` with `parseLayout` so it reads:
```
...Anchor enum, parseHexColor, parseLayout, computeToastRect...
```

- [ ] **Step 3: Run the whole-mod compile gate**

Run the Compile gate command. Expected: exit 0, no output.

- [ ] **Step 4: Run the pure-helper unit tests**

Run the Pure-helper unit-test commands. Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add README.md CLAUDE.md
git commit -m "Update docs for Fluent toast redesign"
```

- [ ] **Step 6: Manual runtime verification (human reviewer)**

Load the mod in Windhawk (Create New Mod → paste → Compile → Enable) and run the spec §7 checklist: each `layout` value (pill/tile/minimal); `showIcon` on/off in Pill and Minimal; Tile always shows its tile; ON accent vs OFF neutral gray; per-key color override tints ON; drop shadow renders without clipping and the surface stays anchored; light and dark theme; multi-monitor "all" mode.

---

## Notes for the implementer

- **`max`:** GDI+ headers (via `windows.h`, no `NOMINMAX`) provide the `max` macro used in the measure helpers. The compile gate confirms this; if it ever fails, replace `max(a, b)` with `(a < b ? b : a)`.
- **Insert glyph (`⎀`) / Scroll glyph (`⤓`):** may render as a missing-glyph box in some fonts. `showIcon` is **off** by default, so this only affects users who opt in; acceptable per spec (fixed glyphs this pass).
- **Acrylic blur** is intentionally out of scope (spec §3.1); the surface is a solid semi-opaque fill.
- Mockups for reference live under `.superpowers/mockups/` (gitignored).
