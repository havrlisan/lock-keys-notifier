# Fullscreen Suppression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in setting that suppresses the lock-key toast while a fullscreen application is in the foreground.

**Architecture:** One new boolean setting (`suppressFullscreen`, default off) read into `g_settings`. A new non-pure detector `IsFullscreenActive()` OR's the shell notification state (`SHQueryUserNotificationState`) with a foreground-window-covers-the-full-monitor fallback. The hook callback skips `RequestToast` when the flag is on and the detector returns true.

**Tech Stack:** C++ (Windhawk single-file mod), Win32 + Shell APIs (`SHQueryUserNotificationState`, `GetForegroundWindow`, `MonitorFromWindow`), bundled Windhawk LLVM/clang toolchain.

## Global Constraints

- `lock-keys-notifier.wh.cpp` must stay fully self-contained — no local `#include`s, system headers only. Do not split it.
- Code inside `// === HELPERS BEGIN ===` / `// === HELPERS END ===` may depend on **only** `<windows.h>`, `<string>`, `<cstdint>`. The detector uses shell APIs, so it goes **after** the END marker. Never edit the marker comment text.
- Every settings key in `==WindhawkModSettings==` must have a matching `Wh_Get*Setting(L"<key>")` read with an identical name.
- Architecture is x86-64 only.
- Primary correctness check (no automated runtime test): whole-mod `-fsyntax-only` compile gate:
  ```bash
  "/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
  ```
- Commit subjects short; do not push.

---

### Task 1: Add the `suppressFullscreen` setting

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — `==WindhawkModSettings==` block (around `lock-keys-notifier.wh.cpp:46-184`), `Settings` struct (`:309-339`), `LoadSettings` (`:395+`).

**Interfaces:**
- Produces: `g_settings.suppressFullscreen` (`bool`) — read by Task 3.

- [ ] **Step 1: Add the setting to the `==WindhawkModSettings==` block**

Pick a sensible location (e.g. just after the four `notify*` toggles). Insert:

```
- suppressFullscreen: false
  $name: Don't show when a fullscreen app is active
  $description: >-
    Skips the toast while a fullscreen application is in the foreground —
    games (DirectX or borderless), fullscreen video, and presentation mode.
    Focus Assist / Do Not Disturb is not affected.
```

- [ ] **Step 2: Add the field to the `Settings` struct**

In `struct Settings`, alongside the other bool toggles (near `bool notifyCaps, notifyNum, notifyScroll, notifyInsert;`), add:

```cpp
    bool suppressFullscreen;
```

- [ ] **Step 3: Read it in `LoadSettings`**

Next to `s.notifyInsert = Wh_GetIntSetting(L"notifyInsert");`, add:

```cpp
    s.suppressFullscreen = Wh_GetIntSetting(L"suppressFullscreen");
```

- [ ] **Step 4: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit 0.

- [ ] **Step 5: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add suppressFullscreen setting"
```

---

### Task 2: Add the `IsFullscreenActive()` detector

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — add the function after `// === HELPERS END ===` (`:304`) and before its first caller (Task 3). Add the `<shlobj_core.h>` include with the other system includes at the top of the file if not already present.

**Interfaces:**
- Produces: `static bool IsFullscreenActive();` — called by Task 3. No parameters; reads live global window/shell state.

- [ ] **Step 1: Ensure the shell header is included**

At the top of the file with the other `#include`s, confirm/add:

```cpp
#include <shlobj_core.h>
```

(Declares `SHQueryUserNotificationState` and `QUERY_USER_NOTIFICATION_STATE`.)

- [ ] **Step 2: Add the detector after the `HELPERS END` marker**

```cpp
// Returns true when a fullscreen app is in the foreground and the toast should be
// suppressed. Not pure (shell + window-manager APIs) — must live after HELPERS END.
static bool IsFullscreenActive() {
    // Layer 1: the same shell signal Windows uses to gate its own toasts.
    QUERY_USER_NOTIFICATION_STATE st;
    if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
        switch (st) {
            case QUNS_BUSY:                     // full-screen (non-D3D) app or presentation settings
            case QUNS_RUNNING_D3D_FULL_SCREEN:  // DirectX exclusive fullscreen
            case QUNS_PRESENTATION_MODE:        // presentation mode
            case QUNS_APP:                      // fullscreen Store app
                return true;
            default:                            // QUNS_QUIET_TIME (Focus Assist),
                break;                          // QUNS_ACCEPTS_NOTIFICATIONS, QUNS_NOT_PRESENT
        }
    }

    // Layer 2: borderless / fullscreen-video fallback — foreground window covers the
    // whole monitor (full rcMonitor, not work area) and is not the desktop.
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
    const RECT& m = mi.rcMonitor;  // full monitor bounds, includes the taskbar area
    return wr.left <= m.left && wr.top <= m.top &&
           wr.right >= m.right && wr.bottom >= m.bottom;
}
```

- [ ] **Step 3: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit 0. If `SHQueryUserNotificationState` / `QUERY_USER_NOTIFICATION_STATE` are reported undeclared, the `<shlobj_core.h>` include from Step 1 is missing or the wrong header — fix before proceeding. (Linkage to `shell32` can't be checked by `-fsyntax-only`; note it for manual testing per the spec's risk section.)

- [ ] **Step 4: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Add IsFullscreenActive detector"
```

---

### Task 3: Wire the suppression check into the hook

**Files:**
- Modify: `lock-keys-notifier.wh.cpp` — `LowLevelKeyboardProc` (`:1088-1110`), specifically the lock-section around `:1101-1104`.

**Interfaces:**
- Consumes: `g_settings.suppressFullscreen` (Task 1), `IsFullscreenActive()` (Task 2).

- [ ] **Step 1: Read the flag under the lock and gate `RequestToast`**

Replace:

```cpp
                EnterCriticalSection(&g_settingsCs);
                bool enabled = KeyEnabled(g_settings, i);
                LeaveCriticalSection(&g_settingsCs);
                if (enabled) RequestToast(i, isOn);
```

with:

```cpp
                EnterCriticalSection(&g_settingsCs);
                bool enabled = KeyEnabled(g_settings, i);
                bool suppressFs = g_settings.suppressFullscreen;
                LeaveCriticalSection(&g_settingsCs);
                // Detector calls Windows APIs — run it outside the lock, and only
                // when the flag is on so it costs nothing when disabled.
                if (enabled && !(suppressFs && IsFullscreenActive())) RequestToast(i, isOn);
```

- [ ] **Step 2: Run the compile gate**

Run:
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```
Expected: no output, exit 0.

- [ ] **Step 3: Commit**

```bash
git add lock-keys-notifier.wh.cpp
git commit -m "Suppress toast when a fullscreen app is active"
```

---

### Task 4: Document the setting in the README

**Files:**
- Modify: `README.md` (and `README.wh.md` if present) — the settings list.

**Interfaces:** none.

- [ ] **Step 1: Locate the settings documentation**

Run:
```bash
grep -rn "notifyInsert\|Scroll Lock\|## Settings\|Settings" README*.md
```
Find where the per-setting list lives.

- [ ] **Step 2: Add an entry for the new setting**

Add a list item matching the existing style, e.g.:

```markdown
- **Don't show when a fullscreen app is active** — when on, skips the toast while a
  fullscreen application is in the foreground (DirectX or borderless games, fullscreen
  video, presentation mode). Off by default. Focus Assist / Do Not Disturb is not affected.
```

- [ ] **Step 3: Commit**

```bash
git add README*.md
git commit -m "Document fullscreen suppression setting"
```

---

## Self-Review

- **Spec coverage:** Setting + default off (Task 1); two-layer detector with exact state list and full-monitor comparison (Task 2); hook check outside the lock, flag-gated (Task 3); README (Task 4); compile-gate risk for `<shlobj_core.h>`/shell32 (Task 2 Step 3). Focus Assist exclusion is encoded by omitting `QUNS_QUIET_TIME` (Task 2). All spec sections covered.
- **Placeholder scan:** none — every code step shows complete code.
- **Type consistency:** `suppressFullscreen` (bool), `IsFullscreenActive()` (`static bool`, no args) used consistently across Tasks 1–3.
