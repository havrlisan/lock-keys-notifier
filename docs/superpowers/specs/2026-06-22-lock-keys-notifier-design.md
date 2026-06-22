# Lock Keys Notifier — Design Spec

**Date:** 2026-06-22
**Author:** Havrlisan
**Status:** Approved, ready for implementation planning

## 1. Overview

A [Windhawk](https://github.com/ramensoftware/windhawk) mod that displays a small,
custom-drawn toast notification whenever a lock key is toggled, showing the new
ON/OFF state. The appearance, position, behavior, and text are heavily
configurable. The mod runs inside `explorer.exe`.

**Supported keys:** Caps Lock, Num Lock, Scroll Lock, and (optionally) Insert.

**Publishing identity:**
- Mod ID: `lock-keys-notifier`
- Display name: `Lock Keys Notifier`
- Author: `Havrlisan`
- GitHub: `https://github.com/havrlisan`
- License: MIT

## 2. Architecture

- **Host process:** injected into `explorer.exe` only (`@include explorer.exe`).
  Single, always-running instance — lightest footprint and a single global hook
  covers the whole system.
- **Key capture:** a low-level keyboard hook,
  `SetWindowsHookEx(WH_KEYBOARD_LL, ...)`, installed on a **dedicated worker
  thread**. That thread owns the hook, the toast window(s), and a message pump.
  A message loop is required on the installing thread for both LL hook delivery
  and `WM_TIMER`. Using our own thread isolates the mod from explorer's threads
  and gives a clean lifecycle.
- **Rendering:** a custom **layered window**
  (`WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
  WS_EX_TRANSPARENT`, `WS_POPUP`), drawn with **GDI+** into a premultiplied ARGB
  bitmap and presented via `UpdateLayeredWindow`. This provides rounded corners,
  anti-aliased custom fonts, per-pixel alpha, and fade. The window is
  **click-through** and never takes focus, and is hidden from the taskbar and
  Alt-Tab.
- **Reuse model:** a single hidden window is created at init. Each toggle updates
  its content, repositions it, shows it, and resets the dismiss timer. Rapid
  presses reuse the same window and reset its timer (no stacking).
  - **Exception:** "all monitors" mode creates one window per monitor (a small
    pool, reused across toggles).

## 3. State Detection

For each monitored virtual key, the LL hook reacts to the **key-down edge only**:

1. Track a per-key "is physically down" flag. Ignore a key-down while the key is
   already marked down (auto-repeat).
2. On a genuine down edge, flip the mod's own tracked ON/OFF boolean for that key
   and signal the UI thread to show the toast with the new state.
3. On key-up, clear the per-key down flag.

Initial state is seeded from `GetKeyState(VK_*) & 1` at init. Self-tracking
avoids the race where `GetKeyState` on the hook thread can lag the actual toggle.
Injected input (`SendInput` / `keybd_event`) also flows through the LL hook, so
programmatic toggles are caught.

**Insert caveat:** the mod reports Insert's raw toggle bit. Its real meaning
(overtype mode) is application-specific, so the displayed state is the OS toggle
state, not any app's overtype mode. Insert notifications are **off by default**.

Monitored virtual keys: `VK_CAPITAL`, `VK_NUMLOCK`, `VK_SCROLL`, `VK_INSERT`.

## 4. Lifecycle

- **`Wh_ModInit`** — load settings; start the worker thread, which creates the
  toast window and installs the LL hook. If hook installation fails, log the
  error and return `FALSE` so Windhawk reports the failure.
- **`Wh_ModSettingsChanged`** — reload settings into a struct guarded by a
  `CRITICAL_SECTION`; applied on the next toast. No live preview.
- **`Wh_ModUninit`** — signal the worker thread to unhook, destroy the window(s),
  `PostQuitMessage`, and join the thread.

Settings are read by the worker thread and written by the Windhawk thread in
`Wh_ModSettingsChanged`; access is serialized with a `CRITICAL_SECTION`.

## 5. Positioning & Theming

- **Anchor:** a 9-point anchor (top-left, top-center, top-right, middle-left,
  center, middle-right, bottom-left, bottom-center, bottom-right) computed within
  the target monitor's **work area** (respects the taskbar), plus `offsetX` /
  `offsetY` pixel offsets.
- **Monitor target:** enum — `active` (monitor of the foreground window, falling
  back to the cursor), `primary`, or `all`.
- **Theme defaults:** when a color setting is left blank, derive it from the
  system theme — light/dark via the `AppsUseLightTheme` registry value, accent via
  `DwmGetColorizationColor`. Any explicit hex value overrides the derived color.

## 6. Settings

Colors are entered as hex strings (e.g. `#1e1e1e`); blank means "use theme".

**Keys**
- `notifyCapsLock` (bool, default `true`)
- `notifyNumLock` (bool, default `true`)
- `notifyScrollLock` (bool, default `true`)
- `notifyInsert` (bool, default `false`)

**Behavior**
- `durationMs` (int, default `1500`) — time fully visible before fade-out
- `monitor` (enum: `active` / `primary` / `all`, default `active`)
- `positionAnchor` (enum: 9 anchor points, default `bottom-center`)
- `offsetX` (int, default `0`)
- `offsetY` (int, default `48`)
- `fadeEnabled` (bool, default `true`)
- `fadeDurationMs` (int, default `150`)
- `soundMode` (enum: `none` / `systemDefault` / `custom`, default `none`)
- `soundFile` (string path, used when `soundMode = custom`)

**Layout / Appearance**
- `autoSize` (bool, default `true`) — fit to text + padding
- `width` (int) — used when `autoSize` is off
- `height` (int) — used when `autoSize` is off
- `padding` (int, default `16`)
- `cornerRadius` (int, default `8`)
- `backgroundColor` (hex string, blank = theme)
- `backgroundOpacity` (int 0–100, default `90`)
- `textColor` (hex string, blank = theme)
- `borderColor` (hex string, blank = none)
- `borderThickness` (int, default `0`)

**Font**
- `fontFamily` (string, default `Segoe UI`)
- `fontSize` (int, default `24`)
- `fontBold` (bool, default `false`)
- `fontItalic` (bool, default `false`)

**Indicator**
- `showIndicator` (bool, default `true`) — small LED dot; ON uses the per-key
  accent color, OFF uses a dim gray.

**Per-key accent**
- `capsAccentColor`, `numAccentColor`, `scrollAccentColor`, `insertAccentColor`
  (hex strings, blank = system accent)

**Text**
- `textTemplate` (string, default `{key}: {state}`)
- `labelOn` (string, default `ON`)
- `labelOff` (string, default `OFF`)
- `nameCaps` (string, default `Caps Lock`)
- `nameNum` (string, default `Num Lock`)
- `nameScroll` (string, default `Scroll Lock`)
- `nameInsert` (string, default `Insert`)

## 7. Pure Helpers

These are deterministic, isolated functions, easy to reason about since an
injected mod cannot be exercised by a normal unit-test harness:

- `parseHexColor(string) -> ARGB` — parses `#rgb` / `#rrggbb` / `#aarrggbb`,
  falling back to a default on malformed input (logged).
- `formatTemplate(template, keyName, stateLabel) -> string` — substitutes
  `{key}` and `{state}`.
- `computeToastRect(anchor, size, offset, workArea) -> RECT` — anchor math.

## 8. Deliverable Files

- **`lock-keys-notifier.wh.cpp`** — the mod. Contains:
  - `// ==WindhawkMod==` metadata block (id, name, description, version,
    author, github, include = `explorer.exe`, architecture `x86-64` only
    (decided 2026-06-22: explorer.exe is 64-bit on modern Windows, so a 32-bit
    build would never be used; x86 dropped as dead weight),
    `compilerOptions: -lgdiplus -ldwmapi -lwinmm -lgdi32 -luser32`).
  - `// ==WindhawkModReadme==` block.
  - `// ==WindhawkModSettings==` block (all settings above, with `$name` /
    `$description` and `$options` for enums).
  - The C++ implementation.
- **`README.md`** — GitHub-facing; mirrors the readme block plus caveats.
- **`LICENSE`** — MIT, `Copyright (c) 2026 Havrlisan`.

## 9. Known Caveats (documented in README)

- Runs in `explorer.exe`; if explorer is not running, notifications pause until it
  restarts.
- Fullscreen exclusive applications (some games) may cover the topmost toast.
- Insert reports the OS toggle bit, not any application's overtype mode.

## 10. Verification

No automated harness exists for an injected mod. Verification is a **manual test
checklist**:

1. Load the mod in Windhawk; confirm it compiles and injects.
2. Toggle Caps / Num / Scroll / Insert; confirm correct ON/OFF text each time.
3. Disable a key; confirm no toast for it.
4. Sweep position anchor + offsets; confirm placement within the work area.
5. Switch monitor mode (active / primary / all) on a multi-monitor setup.
6. Change colors, opacity, corner radius, border, font family/size/bold/italic;
   confirm rendering.
7. Toggle fade on/off and change duration.
8. Enable sound modes.
9. Edit text template, labels, and key names; confirm substitution.
10. Mash a lock key; confirm a single reused toast with a resetting timer.

The pure helpers (§7) can be checked in isolation.
