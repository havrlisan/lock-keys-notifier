# Font weight setting (replaces "Bold text")

**Date:** 2026-06-23
**Status:** Approved, ready for implementation plan

## Problem

The toast text is too thin at the default settings. The only weight control today
is a boolean `fontBold` ("Bold text"), which jumps straight from Regular to Bold
with nothing in between. Users want a finer weight choice, and the out-of-the-box
default should not be the thin Regular look.

## Goal

Replace the boolean `fontBold` setting with a named **Font weight** dropdown that maps
to the standard Windows `FW_*` weights, and default it to **Semibold** so a fresh
install reads well without touching settings.

## Non-goals

- No numeric (100–900) free-entry field — a named dropdown only.
- No per-element weight settings (name vs. state vs. glyph) exposed to the user.
- No change to the italic setting (`fontItalic` stays as-is).

## Design

### Settings block (`==WindhawkModSettings==`)

Remove:

```
- fontBold: false
  $name: Bold text
```

Add:

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

Default `semibold`.

### Pure helper (inside the `HELPERS BEGIN/END` markers)

Mirrors `parseInsertMode`/`parseLayout`. Returns the raw `lfWeight` int. `FW_*` come
from `<windows.h>`, which the markers already permit, so it stays unit-testable.

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

A `parseFontWeight` case is added to `tests/helpers_test.cpp` covering each named
value plus the blank/unknown → `FW_SEMIBOLD` fallback.

### Settings struct / LoadSettings

- `bool fontBold` → `int fontWeight`.
- `s.fontBold = Wh_GetIntSetting(L"fontBold")` →
  `s.fontWeight = parseFontWeight(GetStr(L"fontWeight"))`.

### Font construction (`RenderToast`)

The substantive change. GDI+'s `Font(FontFamily*, size, style, …)` constructor cannot
express arbitrary weights — its `style` bitmask only knows Regular/Bold/Italic. The
three fonts switch to the `Font(const LOGFONTW*)` constructor, which honors
`lfWeight`:

- `lfHeight = -(em pixels)` (negative = character/em height in device pixels, matching
  the current `UnitPixel` em sizes).

> **Implementation note (deviation):** this GDI+ build (bundled MinGW) does not expose
> the single-argument `Font(const LOGFONTW*)` constructor — only `Font(HDC, const LOGFONTW*)`.
> The implementation grabs a transient screen DC (`GetDC(nullptr)` / `ReleaseDC`) to build
> the three fonts.
- `lfWeight` = the chosen weight.
- `lfItalic` = `s.fontItalic`.
- `lfFaceName` = the chosen family, keeping the existing
  `FontFamily::IsAvailable()` probe → fall back to `Segoe UI` when the family is
  unavailable.

Per-font weights:

- **`fontName`** (key name): chosen weight; italic per `fontItalic`.
- **`fontGlyph`** (icon glyph): chosen weight; italic per `fontItalic`.
- **`fontState`** ("ON"/"OFF" label): `max(chosenWeight, FW_BOLD)` — keeps the state
  label as an emphasis element that is never lighter than the body. Semibold body →
  bold label (today's look); Black body → black label. No italic (as today).

### Caveats (accepted)

- A font family only renders the weights it actually ships; Windows snaps to the
  nearest available weight (e.g. Segoe UI: Light/Semilight/Regular/Semibold/Bold/Black).
- Constructing from `LOGFONT` may shift the rendered glyph size by a fraction versus
  the current path; verify the toast still sizes correctly during manual testing.

### Migration

Windhawk drops the removed `fontBold` key automatically. Anyone who previously had
bold enabled lands on the new Semibold default — heavier than Regular, so there is no
regression toward the thin look. Note this in the docs.

## Docs to update

- This spec is the source of truth.
- Update any README / spec text that references the old "Bold text" setting.

## Testing

- **Unit:** `parseFontWeight` cases added to `tests/helpers_test.cpp` (extract →
  compile → run).
- **Compile gate:** whole-mod `-fsyntax-only` clang check passes.
- **Manual (in Windhawk):** load the mod, cycle through each weight, confirm the body
  and state label render at the expected weights and the toast still sizes/positions
  correctly; confirm the Semibold default on a fresh settings reset.
