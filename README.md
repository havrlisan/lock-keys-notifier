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
