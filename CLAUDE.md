# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single [Windhawk](https://github.com/ramensoftware/windhawk) mod that shows a configurable toast when a lock key (Caps/Num/Scroll/Insert) is toggled. The entire shipping artifact is one self-contained file: `lock-keys-notifier.wh.cpp`. It is compiled and injected into `explorer.exe` by Windhawk, not built into a standalone binary.

## The single-file constraint (most important rule)

`lock-keys-notifier.wh.cpp` must stay **fully self-contained — no local `#include`s** — because Windhawk compiles one pasted file. System headers only. Do not split it into multiple source files.

To keep the pure logic testable despite that constraint, the file contains a delimited section:

```
// === HELPERS BEGIN === (pure: no Windhawk/GDI deps; extracted for tests)
...Anchor enum, parseHexColor, parseLayout, computeToastRect...
// === HELPERS END ===
```

`tools/extract-helpers.sh` greps out everything between those exact marker lines into `tests/helpers_generated.h` (gitignored), which `tests/helpers_test.cpp` includes. Rules that follow from this:
- Code inside the markers must depend on **only** `<windows.h>`, `<string>`, `<cstdint>` — no Windhawk APIs (`Wh_*`), no GDI+. Anything else goes **after** the END marker.
- Never change the marker comment text; the extractor matches them literally.

## Commands

The bundled Windhawk LLVM/clang toolchain is the compiler (no separate build system). Run from the repo root in Git Bash; paths contain spaces, keep them quoted.

Whole-mod compile gate (syntax-only — checks compilation, not linking; this is the primary correctness check during development):
```bash
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" @"/c/Program Files/Windhawk/Compiler/compile_flags.txt" -I"/c/Program Files/Windhawk/Compiler/include" -fsyntax-only lock-keys-notifier.wh.cpp
```

Pure-helper unit tests (extract → compile → run; `-static` avoids a Git Bash DLL-path issue when running the exe):
```bash
bash tools/extract-helpers.sh
"/c/Program Files/Windhawk/Compiler/bin/clang++.exe" -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -static -Itests tests/helpers_test.cpp -o tests/helpers_test.exe && ./tests/helpers_test.exe
```

There is no automated runtime test — the mod only runs injected into `explorer.exe`. Runtime behavior (toast appears, fades, positions, multi-monitor, enable/disable cycles) is verified **manually** by loading the file in Windhawk (Create New Mod → Compile → Enable). The manual checklist lives in the design spec (`docs/superpowers/specs/`).

## Architecture (the parts that span the file)

- **Worker thread owns everything live.** `WorkerThreadProc` creates the GDI+ layered toast window(s), installs the `WH_KEYBOARD_LL` hook, and runs the message pump. The hook callback therefore runs **on that same thread**, so it reaches the window with a plain `PostMessageW` (`RequestToast` → `WM_APP_SHOWTOAST`) — no cross-thread marshaling. A message loop is required on this thread for both the LL hook and `WM_TIMER`.
- **Startup handshake.** `StartWorker` creates a manual-reset event; the worker installs the hook, sets `g_hookInstalled`, and `SetEvent`s **unconditionally** before entering the pump. `StartWorker` waits on that event and returns `g_hookInstalled`, so `Wh_ModInit` returns `FALSE` when the hook fails to install. Shutdown posts `WM_APP_QUIT` as a **window** message (reliable; the window is guaranteed created once the worker signaled ready) → `PostQuitMessage`. Preserve this ordering — moving the hook install or the `SetEvent` can reintroduce a hang or a dropped-quit thread leak.
- **State detection.** On the **key-up edge** the hook reads the live toggle bit with `GetKeyState(vk) & 1` and reports that. Key-up (not key-down) is deliberate: the toggle has settled by release, so it sidesteps the key-down `GetKeyState` lag; and reading the real bit every time (rather than self-tracking a flipped bool) means a toggle the hook never observed — e.g. one made while an elevated app held focus, which **UIPI** hides from a medium-integrity hook — cannot desync the displayed state. One key-up per physical press also ignores auto-repeat, so no `g_keyDown`/`g_keyState` tracking is needed. **The elevated-app gap is an unfixable Windows limitation** (every input API is blocked the same way; only a signed UIAccess binary bypasses it, which a JIT-compiled mod can't be) — documented in the spec and READMEs, not something to keep trying to "fix".
- **Settings.** Declared in the `==WindhawkModSettings==` block, read by `LoadSettings` into `g_settings` under `g_settingsCs`. **Every key in the settings block must have a matching `Wh_Get*Setting(L"<key>")` read with an identical name** — a typo silently breaks a setting. String settings go through `GetStr` (which frees them). Readers (`DoShow`, `WM_TIMER`, the hook's enable check) snapshot `g_settings` under the lock.
- **Rendering pipeline.** `RenderToast` draws with GDI+ into a top-down 32bpp DIB section, then **premultiplies alpha in place** before `UpdateLayeredWindow`. Fade is a constant-alpha ramp via `BLENDFUNCTION.SourceConstantAlpha` in `PresentToast` (the per-pixel premultiplied bitmap is not re-rendered during a fade). The window is click-through (`WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`). Each `ToastWindow` stores its own work-area `area` so timer repositioning works correctly in multi-monitor "All" mode.
- **Color settings** are hex strings (`#rgb`/`#rrggbb`/`#aarrggbb`); blank means "derive from the system light/dark theme + accent" (`SystemUsesLightTheme`, `SystemAccentArgb`). `parseHexColor` returns false on blank/invalid, and `ResolveColor` falls back to the theme default.

## Conventions

- Architecture is **x86-64 only** (explorer is 64-bit on modern Windows; 32-bit was intentionally dropped — see the spec).
- `Insert` reports the OS toggle bit, not any app's overtype mode; it is off by default.
- Design spec and implementation plan are under `docs/superpowers/`. Update the spec when making a conscious deviation from it.
- Per the user's global preference: keep commit subjects short; never push without being asked.
