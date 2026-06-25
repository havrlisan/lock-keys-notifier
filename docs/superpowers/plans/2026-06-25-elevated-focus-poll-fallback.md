# Elevated-focus poll fallback — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in-by-default poll fallback so lock-key toggles (Caps/Num/Scroll) made while an elevated app has focus still raise the toast, which the medium-IL `WH_KEYBOARD_LL` hook cannot see.

**Architecture:** A 250 ms `WM_TIMER` on the worker thread polls `GetKeyState(vk)&1` for all four lock keys. A shared `g_lastToggle[]` plus a `ShouldNotify()` helper dedups the hook and poll paths so a normal toggle never double-toasts. The poll routes through the existing `RequestToast → DoShow`, inheriting per-key enable, fullscreen-suppress, layout, and sound.

**Tech Stack:** Single self-contained C++ file (`lock-keys-notifier.wh.cpp`), Windhawk bundled clang/LLVM toolchain, Win32 + GDI+.

## Global Constraints

- **Single file, no local `#include`s.** All changes go in `lock-keys-notifier.wh.cpp`; system headers only.
- **New runtime code goes AFTER the `// === HELPERS END ===` marker.** It touches `g_settingsCs` / `RequestToast` / shared state, so it must not sit inside the pure-helper markers. Never edit the marker comment text.
- **Settings parity:** every settings-block key must have an identically-named `Wh_Get*Setting` read.
- **Primary correctness check:** the whole-mod compile gate (syntax-only). There is no automated runtime test; runtime behavior is verified manually in Windhawk.
- Compile gate command (run from repo root in Git Bash):
  ```bash
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
  ```
- Pure-helper unit tests (must still pass, unaffected by this change):
  ```bash
  bash tools/extract-helpers.sh
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
  ```
- Commit subjects short; never push.

---

### Task 1: Add the `pollElevated` setting

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — settings block (`==WindhawkModSettings==`, after the `suppressFullscreen` entry, ~line 71-76), `struct Settings` (~line 369), `LoadSettings` (~line 458).

**Interfaces:**
- Produces: settings field `bool Settings::pollElevated;`, populated in `LoadSettings`.

- [ ] **Step 1: Add the settings-block entry** after the `suppressFullscreen` block:

```
- pollElevated: true
  $name: Detect toggles under elevated apps
  $description: >-
    Also polls the lock-key state (about every 250 ms) so a toggle made while an
    administrator app has focus is still shown — the one case the keyboard hook
    cannot see. Adds a small detection delay in that case. Caps/Num/Scroll only;
    Insert cannot be detected under an elevated app.
```

- [ ] **Step 2: Add the struct field.** In `struct Settings`, next to `bool suppressFullscreen;`:

```cpp
    bool suppressFullscreen;
    bool pollElevated;
```

- [ ] **Step 3: Read it in `LoadSettings`** next to the `suppressFullscreen` read:

```cpp
    s.suppressFullscreen = Wh_GetIntSetting(L"suppressFullscreen");
    s.pollElevated = Wh_GetIntSetting(L"pollElevated");
```

- [ ] **Step 4: Run the compile gate**

Run the compile-gate command from Global Constraints.
Expected: no errors (exit 0).

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add pollElevated setting"
```

---

### Task 2: Shared last-known state + `ShouldNotify` + startup seed

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — declare `g_lastToggle` and `ShouldNotify` just after `kLockVk` (~line 1147, before `LowLevelKeyboardProc` at 1159); seed in `WorkerThreadProc` after the hook installs (~line 1211-1213).

**Interfaces:**
- Consumes: `g_settingsCs` (line 401), `enum KeyIndex { ... KI_Count }` (line 512), `kLockVk[KI_Count]` (line 1147).
- Produces: `static bool g_lastToggle[KI_Count];` and `static bool ShouldNotify(int ki, bool curOn);` — returns true (and records `curOn`) only when the toggle changed since last reported.

- [ ] **Step 1: Declare state + helper** immediately after the `kLockVk` definition (line 1147):

```cpp
// Last toggle state we reported per key. Shared by the hook (event path) and the
// poll timer (fallback path); ShouldNotify dedups them so a normal toggle, which
// both may observe, raises exactly one toast.
static bool g_lastToggle[KI_Count];

static bool ShouldNotify(int ki, bool curOn) {
    EnterCriticalSection(&g_settingsCs);
    bool changed = (curOn != g_lastToggle[ki]);
    if (changed) g_lastToggle[ki] = curOn;
    LeaveCriticalSection(&g_settingsCs);
    return changed;
}
```

- [ ] **Step 2: Seed `g_lastToggle` at worker startup.** In `WorkerThreadProc`, right after `g_hookInstalled = (g_realHook != nullptr);` and before `if (g_workerReady) SetEvent(g_workerReady);` (~line 1211-1213):

```cpp
    g_hookInstalled = (g_realHook != nullptr);
    if (!g_realHook) Wh_Log(L"keyboard hook install failed");

    // Seed last-known toggle state so the first poll never fires spuriously.
    for (int i = 0; i < KI_Count; ++i)
        g_lastToggle[i] = (GetKeyState(kLockVk[i]) & 1) != 0;

    if (g_workerReady) SetEvent(g_workerReady);
```

(Keep the existing `Wh_Log` and `SetEvent` lines; only insert the seed loop between them.)

- [ ] **Step 3: Run the compile gate**

Expected: no errors. (`ShouldNotify` is defined but not yet called — that's fine; it's `static` and referenced in Tasks 3–4. If an unused-function warning surfaces, it disappears once Task 3 lands; the gate is syntax-only and passes regardless.)

- [ ] **Step 4: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add shared toggle state and ShouldNotify dedup helper"
```

---

### Task 3: Route the hook through `ShouldNotify`

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — `LowLevelKeyboardProc`, the `if (enabled) RequestToast(i, isOn);` line (~line 1179).

**Interfaces:**
- Consumes: `ShouldNotify(int, bool)` from Task 2.

- [ ] **Step 1: Gate the toast on a real state change.** Replace:

```cpp
                if (enabled) RequestToast(i, isOn);
```

with:

```cpp
                if (enabled && ShouldNotify(i, isOn)) RequestToast(i, isOn);
```

- [ ] **Step 2: Run the compile gate**

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Dedup hook path through ShouldNotify"
```

---

### Task 4: Poll timer (constant + start + handler)

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — timer-id defines (~line 1000-1002), `WorkerThreadProc` startup (~after `g_primaryToastHwnd` is set, line 1205, or alongside the seed in Task 2's location), `ToastWndProc` `WM_TIMER` (~line 1073).

**Interfaces:**
- Consumes: `ShouldNotify` (Task 2), `Settings::pollElevated` (Task 1), `KeyEnabled(const Settings&, int)` (line 1150), `kLockVk` (1147), `g_primaryToastHwnd` (534), `g_settingsCs` (401).

- [ ] **Step 1: Add the timer id + interval** next to `FADE_TIMER`/`HOLD_TIMER` (line 1000-1002):

```cpp
#define FADE_TIMER 1
#define HOLD_TIMER 2
#define POLL_TIMER 3
#define FADE_TICK_MS 16
#define POLL_TICK_MS 250
```

- [ ] **Step 2: Start the poll timer at worker startup.** In `WorkerThreadProc`, after `g_primaryToastHwnd = g_toasts.empty() ? nullptr : g_toasts[0].hwnd;` (line 1205):

```cpp
    g_primaryToastHwnd = g_toasts.empty() ? nullptr : g_toasts[0].hwnd;

    // Continuous poll fallback: catches Caps/Num/Scroll toggles made while an
    // elevated app has focus (UIPI hides those from the hook). Runs regardless of
    // toast visibility; the handler no-ops when the setting is off.
    if (g_primaryToastHwnd)
        SetTimer(g_primaryToastHwnd, POLL_TIMER, POLL_TICK_MS, nullptr);
```

- [ ] **Step 3: Handle `POLL_TIMER` in `ToastWndProc`.** At the very top of the `case WM_TIMER: {` block (line 1073), before the `ToastWindow* tw = nullptr;` lookup:

```cpp
    case WM_TIMER: {
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
        // Find which toast owns this hwnd.
        ToastWindow* tw = nullptr;
        ...
```

(Insert only the `if (wParam == POLL_TIMER) { ... return 0; }` block; leave the existing fade/hold logic below it unchanged.)

- [ ] **Step 4: Run the compile gate**

Expected: no errors.

- [ ] **Step 5: Run the pure-helper unit tests** (confirm nothing in the extracted helpers broke):

```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add poll-timer fallback for elevated-focus toggles"
```

---

### Task 5: Update docs to the refined limitation

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — README note (`==WindhawkModReadme==`, the "Toggles made while an elevated app…" bullet, ~line 51-54) and add the setting to the README settings list; hook comment in `LowLevelKeyboardProc` (~line 1166-1170).
- Modify: `CLAUDE.md` — the "State detection" paragraph.

**Interfaces:** none (documentation only).

- [ ] **Step 1: Rewrite the README limitation bullet** (lines 51-54) to:

```
- Toggles made while an elevated (administrator) app has focus are caught by an
  optional poll fallback (on by default, ~250 ms delay): the keyboard hook can't
  see input to higher-integrity windows, so the mod also polls the global lock
  state. This covers Caps/Num/Scroll. Insert is the exception — its toggle state
  isn't readable across the integrity boundary, so an Insert toggle made under an
  elevated app is still missed until the next toggle in a normal app.
```

- [ ] **Step 2: Add the setting to the README settings list.** Find where the settings are listed in the README block and add a line consistent with the existing format, e.g.:

```
- Detect toggles under elevated apps (default on): poll fallback so Caps/Num/Scroll
  still notify when an administrator app has focus.
```

(Match the surrounding list's exact wording/punctuation style; if the README lists settings differently, follow that pattern.)

- [ ] **Step 3: Update the hook comment** (lines 1166-1170). Replace the parenthetical about the elevated gap so it reads that the poll fallback now covers it for Caps/Num/Scroll, e.g.:

```cpp
                // Read the live toggle bit on the key-up edge. The state is settled
                // by release (the key-down read lags). The hook can't see input to a
                // higher-integrity (elevated) window; the poll-timer fallback covers
                // that gap for Caps/Num/Scroll. ShouldNotify dedups the two paths so
                // a normal toggle (seen by both) raises one toast. One key-up per
                // physical press also ignores auto-repeat.
```

- [ ] **Step 4: Update `CLAUDE.md`** — in the "State detection" bullet, replace the sentence asserting the gap is unfixable with:

```
The hook can't observe a toggle made while an elevated app holds focus (UIPI), so
an optional poll-timer fallback (`pollElevated`, on by default) reads the global
toggle state every ~250 ms to cover Caps/Num/Scroll. **Insert stays unrecoverable
in that case** — its toggle isn't readable across the integrity boundary (and it
has no LED), the one genuinely irreducible gap.
```

- [ ] **Step 5: Run the compile gate** (README/comment edits are in the `.cpp`; confirm it still compiles):

Expected: no errors.

- [ ] **Step 6: Commit**

```bash
git add lock-keys-notifier.wh.cpp CLAUDE.md
git commit -m "Document poll fallback and narrowed Insert-only limitation"
```

---

## Manual verification (in Windhawk, after all tasks)

Load the file in Windhawk (Create New Mod → Compile → Enable), then:

- Setting on (default), **normal** case: toggling each of Caps/Num/Scroll/Insert shows exactly **one** toast per press (no hook+poll double-toast).
- Setting on, **elevated** app focused: toggling **Caps**, **Num**, **Scroll** shows the toast within ~250 ms with correct state.
- Setting on, elevated app focused, **Insert**: no toast (documented); toggling Insert again in a normal app shows correctly.
- Setting **off**: hook-only behavior as before; elevated-focus toggles missed for all keys.
- Toggle the setting on↔off at runtime: polling starts/stops with no stray or missing toast.
- Per-key disable (e.g. Num off): suppresses both hook- and poll-originated toasts for that key.

## Self-review

- **Spec coverage:** mechanism=GetKeyState (Task 4 poll uses `GetKeyState`), setting default-on (Task 1), shared state + dedup (Task 2/3), always-on guarded timer (Task 4), Insert exception + docs (Task 5), manual checklist (above). All spec sections mapped.
- **Placeholders:** none — every code step shows exact code; the only judgement step (README settings-list wording) is bounded by "match surrounding format".
- **Type consistency:** `ShouldNotify(int, bool)→bool`, `g_lastToggle[KI_Count]`, `Settings::pollElevated`, `POLL_TIMER`/`POLL_TICK_MS`, `KeyEnabled(const Settings&, int)` used consistently across Tasks 2–4.
