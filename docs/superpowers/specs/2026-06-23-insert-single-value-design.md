# Insert single-value display mode — design

Date: 2026-06-23

## Problem

The Insert "lock" key is unlike Caps/Num/Scroll: its toggle bit means "overtype",
which is app-specific and often meaningless. Showing an accent-vs-neutral **ON/OFF**
state for it implies a global mode that does not really exist. Some users would
rather have Insert simply confirm the key was pressed with a single fixed word
(e.g. "pressed"), with no color that suggests an on/off state.

## Goal

Add an Insert-only option to display a single fixed label in neutral styling on
every Insert press, instead of the ON/OFF treatment. The choice is the user's;
ON/OFF remains the default.

Non-goals: no change to the other three keys; no change to the LL hook, threading,
startup handshake, or the `notifyInsert` gating; no attempt to read any app's real
overtype state (the OS only exposes the toggle bit, and Insert single-value mode
ignores it entirely).

## Settings (two new keys)

Added to the `==WindhawkModSettings==` block:

- `insertDisplayMode: onoff` — dropdown, styled like the existing `layout` dropdown:
  - `onoff: On/Off — accent when on, neutral when off`
  - `single: Single value — one fixed label, neutral`
- `insertSingleLabel: "pressed"` — string; used only in `single` mode.

Each new key gets a matching `Wh_Get*Setting` read with an identical name in
`LoadSettings` (per the project's settings rule). `insertSingleLabel` is read via
`GetStr` (which frees the Windhawk string).

## Pure helper

Inside the `// === HELPERS BEGIN/END ===` markers, next to `parseLayout` (so it
stays dependency-free and unit-testable):

```cpp
enum class InsertMode { OnOff, Single };
inline InsertMode parseInsertMode(const std::wstring& s) {
    if (s == L"single") return InsertMode::Single;
    return InsertMode::OnOff;
}
```

A unit test is added to `tests/helpers_test.cpp` alongside the `parseLayout` tests
(`single` → Single; anything else / blank → OnOff).

## Wiring

- `Settings` struct: add `InsertMode insertDisplayMode;` and
  `std::wstring insertSingleLabel;`.
- `LoadSettings`:
  - `s.insertDisplayMode = parseInsertMode(GetStr(L"insertDisplayMode"));`
  - `s.insertSingleLabel = GetStr(L"insertSingleLabel");`

## Render change

The entire behavioral change lives in `RenderToast`, replacing the two lines that
set `c.isOn` / `c.state`:

```cpp
bool insertSingle = (keyIndex == KI_Insert &&
                     s.insertDisplayMode == InsertMode::Single);
c.isOn  = insertSingle ? false : isOn;                 // force neutral coloring
c.state = insertSingle ? s.insertSingleLabel
                       : (isOn ? s.labelOn : s.labelOff);
```

Forcing `c.isOn = false` routes all three layouts (Pill, Tile, Minimal) through
their existing neutral branch in `MakePillColors` (and the Tile glyph through its
`c.fg` branch). "No color adaptation" therefore falls out for free — no per-layout
drawing edits needed.

Because the LL hook already fires once per physical key-up regardless of toggle
direction, `single` mode naturally shows the label on **every** Insert press; no
state-direction filtering is added.

## Verification

- Whole-mod `-fsyntax-only` compile gate.
- Helper unit tests (extract → compile → run), including the new `parseInsertMode`
  cases.
- Manual (per the usual runtime checklist): toggle Insert in both `onoff` and
  `single` modes, on dark and light themes, across all three layouts; confirm the
  single label is always neutral and the configurable label text takes effect.

## Docs

Update the README feature list to mention the Insert single-value option.
