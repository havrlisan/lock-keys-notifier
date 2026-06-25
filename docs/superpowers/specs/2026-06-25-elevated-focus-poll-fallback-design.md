# Detect lock-key toggles under elevated (administrator) apps

**Date:** 2026-06-25
**Status:** Approved, ready for implementation plan

## Problem

The mod runs at **medium integrity**. Its `WH_KEYBOARD_LL` hook never fires for
input directed at a **high-integrity (elevated/administrator)** foreground window —
UIPI blocks a lower-IL process from *receiving* that input. So a lock-key toggle made
while, say, an admin Command Prompt or Task Manager has focus is missed, and the
displayed state can desync until the next toggle in a normal app.

This was previously documented as an **unfixable** Windows limitation. Feasibility
research + an empirical probe (see below) showed that conclusion was too broad: the
input *event* is hidden, but the lock-key *toggle state* is global and readable.

## Feasibility evidence (why this is now fixable)

A standalone probe (`lockstate_probe.cpp`) polled the lock state every 100 ms from a
plain medium-IL background thread while an elevated terminal held focus, comparing
several APIs. Observed on the dev machine (Windows 11 Pro 26100):

- **`GetAsyncKeyState`** went silent under elevated focus — confirming UIPI *was*
  active and *did* block the input-event read (its "pressed-bit" only appeared when a
  normal window was focused). This validates the test conditions.
- **`GetKeyState`, `GetKeyboardState`, and the keyboard-LED IOCTL
  (`IOCTL_KEYBOARD_QUERY_INDICATORS`)** all **tracked the toggle anyway** for
  **Caps, Num, Scroll** — the toggle latch is global system state and reading it is
  not UIPI-gated.
- **Insert is the exception.** Re-testing all four keys, Insert did **not** update
  under elevated focus. Its toggle is not backed by global driver/LED state the way
  Caps/Num/Scroll are; it stays tied to the UIPI-partitioned input layer. The LED
  IOCTL can't read Insert either (no LED). **Insert under elevated focus is
  irreducible** and remains unrecoverable.

Research also ruled out the other avenues, so polling is the only viable fix:

- **High integrity / UIAccess:** unreachable. The Windhawk engine runs as the *user*
  (not SYSTEM); the tool-mod process is medium-IL with no supported elevation; and a
  JIT-compiled mod can never be Authenticode-signed + installed in a secure location,
  which UIAccess requires.
- **Raw Input, `WH_KEYBOARD`, DirectInput, `GetAsyncKeyState`:** all explicitly in
  the "UIAccess-only" cross-integrity input list — same gate as the LL hook.

## Goal

Add an **opt-in-by-default** poll fallback so that toggles of **Caps / Num / Scroll**
made while an elevated app is focused still raise the toast (within the poll
interval). Keep the existing hook for instant response in the normal case. No
double-toasts. Insert behaves as before (normal case works; elevated case documented
as unfixable).

## Non-goals

- **Not** using the LED IOCTL. `GetKeyState` is the chosen source: it covers all four
  keys for the normal case, is the *same* source the hook already reads (so the two
  paths can never disagree), and needs no device handle / `DefineDosDevice` plumbing.
  The LED IOCTL is recorded here as a deliberately-unused fallback (YAGNI; it also
  can't do Insert).
- **No configurable poll interval** — fixed at 250 ms (a `GetKeyState` poll is
  effectively free; exposing a knob is YAGNI).
- **No attempt to recover Insert under elevated focus** — shown to be impossible.

## Design

### Chosen mechanism

Poll `GetKeyState(vk) & 1` for the four lock VKs on a timer running on the worker
thread (which already owns the message pump the hook and `WM_TIMER` require).

### Settings block (`==WindhawkModSettings==`)

Add one boolean, default **`true`** (this is the fix; the cost is negligible). Place
it near the other behavior toggles (e.g. after `suppressFullscreen`):

```
- pollElevated: true
  $name: Detect toggles under elevated apps
  $description: >-
    Also polls the lock-key state (about every 250 ms) so a toggle made while an
    administrator app has focus is still shown — the one case the keyboard hook
    cannot see. Adds a small detection delay in that case. Caps/Num/Scroll only;
    Insert cannot be detected under an elevated app.
```

### Settings struct / `LoadSettings`

- Add `bool pollElevated;` to `Settings`.
- Read it in `LoadSettings`, matching the existing bool convention and the exact key
  name: `s.pollElevated = Wh_GetIntSetting(L"pollElevated");`

### Shared last-known state + unified dedup

Add, alongside `g_settings` / `g_settingsCs`:

```cpp
static bool g_lastToggle[KI_Count];   // last reported toggle state per key
```

Guard it with the existing `g_settingsCs` (it's already the snapshot lock and these
reads/writes are tiny). One helper unifies both detection paths:

```cpp
// True (and records the new state) only when the toggle actually changed since we
// last reported it. Dedups the hook vs. poll paths regardless of which sees it first.
static bool ShouldNotify(int ki, bool curOn) {
    EnterCriticalSection(&g_settingsCs);
    bool changed = (curOn != g_lastToggle[ki]);
    if (changed) g_lastToggle[ki] = curOn;
    LeaveCriticalSection(&g_settingsCs);
    return changed;
}
```

Seed `g_lastToggle` from live state at worker startup (before the pump, after the
hook installs) so the first poll never fires spuriously:

```cpp
for (int i = 0; i < KI_Count; ++i)
    g_lastToggle[i] = (GetKeyState(kLockVk[i]) & 1) != 0;
```

### Hook path (`LowLevelKeyboardProc`)

Currently the hook is stateless and calls `RequestToast` on every lock-key key-up.
Route it through `ShouldNotify` so it can't double-fire with the poll:

```cpp
bool isOn = (GetKeyState(kLockVk[i]) & 1) != 0;
EnterCriticalSection(&g_settingsCs);
bool enabled = KeyEnabled(g_settings, i);
LeaveCriticalSection(&g_settingsCs);
if (enabled && ShouldNotify(i, isOn)) RequestToast(i, isOn);
```

Each physical lock-key press flips the toggle, so a key-up still maps to exactly one
state change → one toast, as today. (Auto-repeat produces no extra key-ups, so no
change there.)

### Poll timer

- New timer id `POLL_TIMER` (distinct from `FADE_TIMER` / `HOLD_TIMER`) and constant
  `POLL_TICK_MS = 250`.
- The poll timer is armed via `UpdatePollTimer()` (worker thread only), which arms
  it only when `pollElevated` is on and `KillTimer`s when it goes off. It runs
  independent of whether any toast is visible, but **not** when the feature is
  disabled (see rationale below). It uses `SetCoalescableTimer` with a
  `POLL_TOLERANCE_MS = 100` tolerance so Windows can batch the wakeup with other
  system timers — the poll is default-on, so this cuts its perpetual power cost at
  the price of up to ~100 ms extra worst-case detection latency.
- Handle it in `ToastWndProc`'s `WM_TIMER`, **branching on `wParam == POLL_TIMER`
  before** the per-toast fade/hold lookup (the poll is not toast-specific):

```cpp
if (wParam == POLL_TIMER) {
    Settings s;
    EnterCriticalSection(&g_settingsCs);
    s = g_settings;
    LeaveCriticalSection(&g_settingsCs);
    if (s.pollElevated) {
        for (int i = 0; i < KI_Count; ++i) {
            bool isOn = (GetKeyState(kLockVk[i]) & 1) != 0;
            if (KeyEnabled(s, i) && ShouldNotify(i, isOn)) RequestToast(i, isOn);
        }
    }
    return 0;
}
```

**Why a conditional timer** (started/stopped to match the setting), revised after
PR review: an always-running 250 ms timer is **not** free — it wakes the CPU 4×/s
forever and keeps the system out of deeper idle/power states even for users who
disabled the feature (the same "unconditional cost" the warm-up review flagged). So
the timer exists only while `pollElevated` is on **and** at least one *pollable* key
(Caps/Num/Scroll) notifies — Insert can't be recovered under elevated focus, so
Insert alone is no reason to run it. `SetTimer`/`KillTimer` must run on
the window-owning worker thread, but `WhTool_ModSettingsChanged` runs on a different
thread, so it marshals via `PostMessageW(g_primaryToastHwnd, WM_APP_UPDATEPOLL, 0, 0)`;
the worker handles that message by calling `UpdatePollTimer()`. The `WM_TIMER` branch
keeps its `pollElevated` re-check as cheap insurance for the brief window before a
disable's `KillTimer` lands.

Because the poll routes through the same `RequestToast → DoShow`, it automatically
inherits the per-key enable check, fullscreen suppression, layout, multi-monitor
targeting, and sound — no duplication.

### Re-seed when the poll is enabled at runtime

The hook keeps `g_lastToggle` current for toggles it *can* see, but a toggle made
under elevated focus *while polling was off* leaves it stale. `UpdatePollTimer()`
therefore calls `SeedToggleState()` before arming, so enabling the feature means
"detect from now on" rather than retroactively announcing a past toggle the user no
longer expects a toast for. (Earlier this spec accepted that one corrective toast as
a resync; the conditional-timer rework gives a clean arm point, so suppressing it is
both easy and the less surprising behavior.)

## Single-file / helpers constraint

`ShouldNotify`, the poll handler, and `g_lastToggle` touch `g_settingsCs` /
`RequestToast` (Windhawk + shared state), so they live **after** the `HELPERS END`
marker with the rest of the runtime code — not inside the pure-helper markers. No new
extracted-helper unit tests; covered by the compile gate + manual checklist. The
`lockstate_probe.cpp` experiment is a throwaway diagnostic, not shipped.

## Docs to update

Revise the previously-"unfixable" statements to the narrower, accurate framing — the
**hook** can't see elevated-focus input, but the poll fallback closes the gap for
**Caps/Num/Scroll** within ~250 ms; only **Insert** stays unrecoverable under an
elevated app.

- **README note** (`==WindhawkModReadme==`, the "Toggles made while an elevated app
  has focus…" bullet): rewrite to describe the default-on poll fallback, its ~250 ms
  detection delay, and the remaining Insert-only limitation. Add the new setting to
  the README settings list.
- **Hook comment** in `LowLevelKeyboardProc` (the "a toggle we never saw … UIPI hides
  from this hook" comment): update to mention the poll fallback covers that case for
  Caps/Num/Scroll.
- **CLAUDE.md** "State detection" paragraph: replace "The elevated-app gap is an
  unfixable Windows limitation" with the refined statement (poll fallback for
  Caps/Num/Scroll; Insert remains the irreducible case) and note the `pollElevated`
  setting.

## Testing

- **Unit:** none new — the poll/dedup logic touches shared runtime state and lives
  outside the pure-helper markers.
- **Compile gate:** whole-mod `-fsyntax-only` clang check passes.
- **Manual (in Windhawk):**
  - Setting on (default), **normal** case: toggling Caps/Num/Scroll/Insert shows
    exactly **one** toast per press (no double-toast from hook + poll).
  - Setting on, **elevated** app focused: toggling **Caps**, **Num**, **Scroll**
    shows the toast within ~250 ms with the correct state. (We empirically confirmed
    Caps/Num/Scroll; verify all three in the real mod.)
  - Setting on, elevated app focused, **Insert**: no toast (documented limitation);
    toggling Insert again in a normal app shows correctly.
  - Setting **off**: behaves exactly as before — no polling, hook-only; elevated-focus
    toggles are missed for all keys.
  - Toggle the setting on↔off at runtime (Windhawk re-applies settings) and confirm
    polling starts/stops without a stray or missing toast.
  - Per-key disable (e.g. Num off) still suppresses both hook- and poll-originated
    toasts for that key.
