# Suppress toast when a fullscreen app is active

**Date:** 2026-06-23
**Status:** Approved, ready for implementation plan

## Problem

When a game or fullscreen video is focused, the lock-key toast still draws over it
— an unwanted interruption. Other tools (e.g. FluentFlyout's "Disable Flyouts if a
DirectX fullscreen program is detected") offer an opt-out. There is no such option
today.

## Goal

Add a single opt-in setting that suppresses the toast while a fullscreen application
is active. Detection should match what users actually mean by "fullscreen": true
DirectX exclusive fullscreen, fullscreen Store apps, presentation mode, **and**
borderless-windowed fullscreen / fullscreen video — the last of which a DirectX-only
check misses.

## Non-goals

- **No Focus Assist / Do Not Disturb coupling.** `QUNS_QUIET_TIME` is intentionally
  *not* a suppression trigger — the toast still shows during Focus Assist. (Decided
  during brainstorming; a separate toggle for this is out of scope.)
- No per-key fullscreen behavior — one global toggle, not four.
- No allow-list of specific apps/processes.

## Design

### Settings block (`==WindhawkModSettings==`)

Add one boolean, default `false` (preserves current behavior; opt-in):

```
- suppressFullscreen: false
  $name: Don't show when a fullscreen app is active
  $description: >-
    Skips the toast while a fullscreen application is in the foreground —
    games (DirectX or borderless), fullscreen video, and presentation mode.
    Focus Assist / Do Not Disturb is not affected.
```

### Settings struct / LoadSettings

- Add `bool suppressFullscreen;` to `Settings`.
- Read it in `LoadSettings`: `s.suppressFullscreen = Wh_GetIntSetting(L"suppressFullscreen");`
  (match the existing bool-read convention used by the other bool settings; the key
  name must match the settings block exactly).

### Detector (after the `HELPERS END` marker — NOT pure)

The detector calls shell + window-manager APIs, so it cannot live inside the
`HELPERS BEGIN/END` markers (those permit only `<windows.h>`, `<string>`,
`<cstdint>`). It goes after the END marker, alongside the other GDI/Windows code, and
is covered by the compile gate + manual checklist rather than the unit harness.

Two layers, OR'd together:

```cpp
// Returns true when a fullscreen app should suppress the toast.
static bool IsFullscreenActive() {
    // Layer 1: shell notification state — the same signal Windows uses to gate
    // its own toasts. SHQueryUserNotificationState lives in <shellapi.h>.
    QUERY_USER_NOTIFICATION_STATE st;
    if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
        switch (st) {
            case QUNS_BUSY:                  // full-screen (non-D3D) app or presentation settings
            case QUNS_RUNNING_D3D_FULL_SCREEN:
            case QUNS_PRESENTATION_MODE:
            case QUNS_APP:                   // fullscreen Store app
                return true;
            // QUNS_QUIET_TIME (Focus Assist), QUNS_ACCEPTS_NOTIFICATIONS,
            // QUNS_NOT_PRESENT -> not fullscreen, fall through to layer 2.
            default:
                break;
        }
    }

    // Layer 2: borderless / fullscreen-video fallback. The foreground window covers
    // its entire monitor (full rcMonitor, not the work area) and is not the desktop.
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    if (fg == GetShellWindow() || fg == GetDesktopWindow()) return false;
    WCHAR cls[16];
    int n = GetClassNameW(fg, cls, 16);
    std::wstring c(cls, n > 0 ? n : 0);
    if (c == L"Progman" || c == L"WorkerW") return false;  // desktop

    RECT wr;
    if (!GetWindowRect(fg, &wr)) return false;
    HMONITOR mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfo(mon, &mi)) return false;
    const RECT& m = mi.rcMonitor;  // FULL monitor bounds, includes taskbar area
    return wr.left <= m.left && wr.top <= m.top &&
           wr.right >= m.right && wr.bottom >= m.bottom;
}
```

Notes:
- Comparing against `rcMonitor` (full bounds) rather than `rcWork` distinguishes true
  fullscreen (covers the taskbar) from a merely maximized window (leaves the taskbar
  visible, so its rect equals the work area, not the monitor).
- `<=` / `>=` (not `==`) tolerates apps that extend a pixel past the monitor edge.

### Suppression check (`DoShow`)

> **Deviation (2026-06-23, post-submission review):** the check was moved **out of**
> `LowLevelKeyboardProc` and into `DoShow`. An `WH_KEYBOARD_LL` callback runs
> synchronously and blocks *all* system input until it returns, so the shell +
> window/monitor queries should not sit on that path. The hook now only marshals
> (`if (enabled) RequestToast(i, isOn);`); the suppress decision runs in `DoShow`,
> which executes on the worker thread *after* the hook has returned, so it no longer
> blocks input. `GetKeyState` stays in the hook (it's cheap).

`DoShow` already snapshots `g_settings` into a local `s` under `g_settingsCs`. Right
after that snapshot, drop the toast when suppression applies:

```cpp
// in LowLevelKeyboardProc — just marshal, don't decide:
if (enabled) RequestToast(i, isOn);

// at the top of DoShow, after the settings snapshot:
if (s.suppressFullscreen && IsFullscreenActive()) return;
```

`DoShow` runs once per toast event on the worker thread, so the detector is called at
most once per physical key press — cheap, correctly threaded
(`SHQueryUserNotificationState` / `GetForegroundWindow` query global state and are
safe from any thread), and off the input-blocking hook path.

## Risks / to verify at the compile gate

- `SHQueryUserNotificationState` and `QUERY_USER_NOTIFICATION_STATE`: linkage to
  `shell32` can't be checked by `-fsyntax-only`, so it remains a manual-test concern.

> **Header correction (2026-06-23, post-submission review):** an earlier note here
> claimed `SHQueryUserNotificationState` had to come from `<shlobj.h>` because the
> MinGW-w64 toolchain lacks `<shlobj_core.h>`. That was wrong: in this toolchain the
> symbol and `QUERY_USER_NOTIFICATION_STATE` are declared in
> `Compiler/include/shellapi.h` (verified by `grep`), so the implementation includes
> `<shellapi.h>` and the `<shlobj.h>` include was removed as unused. No explicit
> `shell32` pragma was needed for the syntax-only gate.

## Docs to update

- This spec is the source of truth.
- Add the new setting to the README settings list.

## Testing

- **Unit:** none — the detector depends on live shell/window state and stays outside
  the pure-helper markers. No new `helpers_test.cpp` cases.
- **Compile gate:** whole-mod `-fsyntax-only` clang check passes (also confirms the
  `<shellapi.h>` include resolves `SHQueryUserNotificationState`).
- **Manual (in Windhawk):**
  - Setting off (default): toast behaves exactly as today in all cases.
  - Setting on, DirectX-exclusive-fullscreen game focused → no toast on lock-key
    toggle; alt-tab back to desktop → toast returns.
  - Setting on, borderless-fullscreen game or browser fullscreen video focused → no
    toast.
  - Setting on, merely *maximized* window focused (taskbar visible) → toast still
    shows (not treated as fullscreen).
  - Setting on, Focus Assist enabled but no fullscreen app → toast still shows
    (Focus Assist intentionally excluded).
  - Multi-monitor: fullscreen app on monitor A suppresses; behavior matches the
    foreground window's monitor.
