# Lock Keys Notifier — Fluent Toast Redesign

**Date:** 2026-06-22
**Author:** Havrlisan
**Status:** Approved, ready for implementation planning

This spec covers a **visual redesign** of the toast only. It builds on
`2026-06-22-lock-keys-notifier-design.md`; everything in that document not
contradicted here still holds. The worker-thread/hook architecture, startup
handshake, key-up state detection, multi-monitor logic, fade/sound, and the
pure-helper test seam are **unchanged**.

## 1. Motivation

The shipped toast is a flat filled rounded-rect with a solid state dot and a
single `"{key}: {state}"` string. It reads as a debug overlay, not an OS
notification. This redesign gives it a Windows 11 **Fluent** look — soft drop
shadow, translucent surface, rounded corners, accent-colored state — and
introduces structured, user-selectable layouts.

## 2. Visual model

### 2.1 Layouts

A new `layout` setting selects one of three structures. Default: **Pill**.

- **Pill** (default) — one line: `[icon] Name [state pill]`.
- **Tile** — a rounded icon tile on the left + two stacked text lines
  (Name on top, state label below).
- **Minimal** — a leading glyph + Name on one line; state is conveyed by
  **coloring the state word** (no pill).

All three share one Fluent **surface**: translucent dark/light fill (per
theme), 1px hairline border, rounded corners, and a soft drop shadow.

### 2.2 State (ON vs OFF)

Treatment "color pill":

- **ON** — state element uses the **accent color** (see 2.3): a filled pill
  (Pill/Tile) or colored word (Minimal) showing the **ON label**.
- **OFF** — state element is a **neutral gray** pill/word showing the **OFF
  label**. OFF is deliberately calm — never red. (Caps-off etc. is usually the
  desired state, so OFF must not read as an error.)

The mod fires on **both** toggle directions, so OFF is a normal, intentional
state, styled as such.

### 2.3 Color

- **ON color default:** the **Windows accent** (`SystemAccentArgb`).
- **OFF color:** a fixed neutral gray derived from the theme.
- **Per-key overrides:** the existing `capsAccentColor` / `numAccentColor` /
  `scrollAccentColor` / `insertAccentColor` settings now tint the **ON pill /
  word** for that key (previously they tinted the indicator dot). Blank =
  system accent.

### 2.4 Icon

- New `showIcon` toggle. **Default: off** (clean, no icon).
- When **on**, a per-key **symbol glyph** is drawn: `⇪` Caps, `#` Num,
  `⤓` Scroll, `⎀` Insert. Rendered as text in the toast font (with the same
  font-availability fallback used for the label).
- Scope: `showIcon` controls the leading glyph in **Pill** and **Minimal**.
  The **Tile** layout always renders its icon tile — the tile is the defining
  element of that layout — so it ignores `showIcon`.

## 3. Rendering changes (`RenderToast`)

The function keeps its current shape — measure, (re)create DIB, draw with GDI+,
premultiply alpha — with these changes:

1. **Shared setup (layout-agnostic):** resolve theme/accent, surface fill,
   border, shadow, and ON/OFF state color once.
2. **Per-layout measure + draw:** branch on `layout` into a routine that
   measures its pieces (glyph, name, state pill/word) and draws them. Reuse the
   existing rounded-rect `GraphicsPath` helper for the surface, the state pill,
   and the Tile's icon tile.
3. **Multi-run text measurement:** auto-size sums the measured widths of the
   present pieces (glyph + gaps + name + gap + pill) plus per-layout padding,
   instead of measuring one combined string.
4. **Drop shadow (new primitive, configurable):** the layered window/DIB is
   expanded by a shadow margin on all sides; a soft shadow is rendered beneath
   the surface via layered-alpha falloff (successive rounded-rect fills of
   growing size and decreasing alpha). The surface is inset within the margin.
   `computeToastRect` continues to position by the **visible surface** rect, not
   the shadow-expanded bounds — the margin must not shift the anchored position.
   (Implementation note: account for the margin when placing the window vs. the
   surface.) The shadow's spread, per-layer alpha, vertical offset, and color
   are driven by settings (see §4), so the margin is **computed from those**
   (`ceil(size) + abs(offsetY) + 1`), not a fixed constant. When the shadow is
   disabled the falloff is skipped and the margin collapses to a small AA-safety
   pad; positioning is unaffected because placement already subtracts the margin.
5. **Premultiply step** is unchanged (runs over the full expanded DIB).

### 3.1 Surface: solid now, acrylic later

The surface is a **solid semi-opaque** fill (the current approach, honoring
`backgroundColor` + `backgroundOpacity`). No live blur of content behind the
window. The code is structured so a future **acrylic blur** (opt-in setting via
the undocumented `SetWindowCompositionAttribute` /
`ACCENT_ENABLE_ACRYLICBLURBEHIND`) can be added without reworking the layout/
draw code. Acrylic is **out of scope** for this pass.

## 4. Settings changes

### Added
- `layout` — `pill` (default) / `tile` / `minimal`.
- `showIcon` — bool, default `false`. Shows the per-key symbol glyph
  (Pill/Minimal).
- `shadowEnabled` — bool, default `true`. When off, no shadow is drawn and the
  DIB margin collapses.
- `shadowSize` — int px, default `13` (range 0–40). The shadow's outward spread
  past the surface edge.
- `shadowOpacity` — int, default `40` (range 0–100). Maps to per-layer alpha as
  `alpha = opacity × 25 ÷ 100`, so `40` reproduces the original fixed look.
- `shadowOffsetY` — int px, default `4` (range −20–20). Vertical drop offset;
  negative casts the shadow upward.
- `shadowColor` — hex string, default blank = black. RGB only; any alpha byte in
  the hex is ignored because `shadowOpacity` is the single darkness control.

### Removed
- `showIndicator` — the indicator dot is gone; `showIcon` replaces it.
- `textTemplate` — the structured layouts compose name + state from separate
  elements (`nameCaps/…` and `labelOn`/`labelOff`), so a single
  `{key}: {state}` template no longer fits. Dropped, no legacy fallback.

### Repurposed
- `capsAccentColor` / `numAccentColor` / `scrollAccentColor` /
  `insertAccentColor` — now tint the **ON state pill/word** per key (was the dot
  color). Semantics (blank = system accent) unchanged.

### Retuned defaults
- `cornerRadius` 8 → **10** (softer, more Fluent).
- Padding is applied per layout (kept sensible; `padding` remains the base
  knob).

### Unchanged
Per-key enable, `durationMs`, `monitor`, `positionAnchor`, offsets, `fade*`,
`sound*`, `autoSize` / `width` / `height`, `backgroundColor`,
`backgroundOpacity`, `textColor`, `borderColor`, `borderThickness`, font
settings, `labelOn` / `labelOff`, `nameCaps` / `nameNum` / `nameScroll` /
`nameInsert`.

## 5. Test seam impact

The pure-helper section (`parseHexColor`, `formatTemplate`, `computeToastRect`,
`Anchor`) stays within its markers and its `<windows.h>/<string>/<cstdint>`-only
constraint.

- `formatTemplate` loses its only caller (`textTemplate` dropped). It may be
  removed along with its test, **or** retained if `labelOn`/`labelOff` keep a
  placeholder use; the plan decides. If a `layout` enum + parse needs a pure
  home, it can live in this section with a unit test, mirroring `Anchor`.
- `computeToastRect` is unchanged in signature; the shadow margin is handled by
  the caller (window vs. surface placement), not by this helper.

## 6. Out of scope

- Acrylic / live blur (designed-for, not built).
- Animation beyond the existing fade.
- Changes to detection, lifecycle, monitors, sound, or positioning logic.
- New per-key glyph customization (glyphs are fixed in this pass).

## 7. Manual verification additions

Extend the existing manual checklist with: each `layout` value renders
correctly; `showIcon` on/off in Pill and Minimal; Tile always shows its tile;
ON uses accent and OFF is neutral gray; per-key color override tints the ON
state; drop shadow renders without clipping and the surface stays correctly
anchored (shadow margin doesn't shift position); light and dark theme;
multi-monitor "all" mode.
