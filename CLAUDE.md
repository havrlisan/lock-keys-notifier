# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single [Windhawk](https://github.com/ramensoftware/windhawk) mod that shows a configurable toast when a lock key (Caps/Num/Scroll/Insert) is toggled. The entire shipping artifact is one self-contained file: `lock-keys-notifier.wh.cpp`. It is a [tool mod](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process) (`@include windhawk.exe`): Windhawk compiles it and runs it in a dedicated `windhawk.exe` process, not injected into `explorer.exe` and not built into a standalone binary. The mod's lifecycle entry points are therefore `WhTool_ModInit`/`WhTool_ModSettingsChanged`/`WhTool_ModUninit`; the `Wh_Mod*` launcher boilerplate at the bottom of the file (the verbatim "Do not modify" block) spawns/registers the dedicated process and must not be edited.

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

There is no automated runtime test — the mod only runs when loaded by Windhawk (in its dedicated `windhawk.exe` tool process). Runtime behavior (toast appears, fades, positions, multi-monitor, enable/disable cycles) is verified **manually** by loading the file in Windhawk (Create New Mod → Compile → Enable). The manual checklist lives in the design spec (`docs/superpowers/specs/`).

## Architecture (the parts that span the file)

- **Worker thread owns everything live.** `WorkerThreadProc` creates the GDI+ layered toast window(s), installs the `WH_KEYBOARD_LL` hook, and runs the message pump. The hook callback therefore runs **on that same thread**, so it reaches the window with a plain `PostMessageW` (`RequestToast` → `WM_APP_SHOWTOAST`) — no cross-thread marshaling. A message loop is required on this thread for both the LL hook and `WM_TIMER`.
- **Startup handshake.** `StartWorker` creates a manual-reset event; the worker installs the hook, sets `g_hookInstalled`, and `SetEvent`s **unconditionally** before entering the pump. `StartWorker` waits on that event and returns `g_hookInstalled`, so `Wh_ModInit` returns `FALSE` when the hook fails to install. Shutdown posts `WM_APP_QUIT` as a **window** message (reliable; the window is guaranteed created once the worker signaled ready) → `PostQuitMessage`. Preserve this ordering — moving the hook install or the `SetEvent` can reintroduce a hang or a dropped-quit thread leak.
- **State detection.** On the **key-up edge** the hook reads the live toggle bit with `GetKeyState(vk) & 1` and reports that. Key-up (not key-down) is deliberate: the toggle has settled by release, so it sidesteps the key-down `GetKeyState` lag; and reading the real bit every time (the displayed state always comes from `GetKeyState`, never from a self-flipped bool) means the hook can't desync. One key-up per physical press also ignores auto-repeat.
- **Elevated-app gap + poll fallback.** A medium-integrity hook can't *receive* input while an elevated (high-integrity) app holds focus (**UIPI**), so the hook misses toggles made there. But the lock *toggle state* is global and readable, so an opt-in-by-default poll timer (`pollElevated`, ~250 ms, on the worker thread) reads `GetKeyState` for all four keys and covers **Caps/Num/Scroll**. **Insert stays unrecoverable** under an elevated app — its toggle isn't maintained across the integrity boundary (and it has no LED), the one genuinely irreducible gap; the `IOCTL_KEYBOARD_QUERY_INDICATORS` LED read can't help Insert either. The hook and the poll share `g_lastToggle[]`; `ShouldNotify` (under `g_settingsCs`) returns true only on an actual change, so a normal toggle seen by *both* paths raises exactly one toast. `g_lastToggle` is dedup state, **not** the source of truth — the toast's ON/OFF always comes from the live `GetKeyState` read. (For the full feasibility evidence — empirical probe results, why Raw Input / UIAccess / high-integrity are all dead ends — see `docs/superpowers/specs/2026-06-25-elevated-focus-poll-fallback-design.md`.)
- **Settings.** Declared in the `==WindhawkModSettings==` block, read by `LoadSettings` into `g_settings` under `g_settingsCs`. **Every key in the settings block must have a matching `Wh_Get*Setting(L"<key>")` read with an identical name** — a typo silently breaks a setting. String settings go through `GetStr` (which frees them). Readers (`DoShow`, `WM_TIMER`, the hook's enable check) snapshot `g_settings` under the lock.
- **Rendering pipeline.** `RenderToast` draws with GDI+ into a top-down 32bpp DIB section, then **premultiplies alpha in place** before `UpdateLayeredWindow`. Fade is a constant-alpha ramp via `BLENDFUNCTION.SourceConstantAlpha` in `PresentToast` (the per-pixel premultiplied bitmap is not re-rendered during a fade). The window is click-through (`WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`). Each `ToastWindow` stores its own work-area `area` so timer repositioning works correctly in multi-monitor "All" mode.
- **Color settings** are hex strings (`#rgb`/`#rrggbb`/`#aarrggbb`); blank means "derive from the system light/dark theme + accent" (`SystemUsesLightTheme`, `SystemAccentArgb`). `parseHexColor` returns false on blank/invalid, and `ResolveColor` falls back to the theme default.

## Conventions

- **No `@architecture` restriction** (builds both x86 and x86-64). The tool-mod host is `windhawk.exe`, which is **32-bit (x86)** on all systems (it ships a separate `windhawk-x64-helper.exe` for 64-bit work), and the spawned `windhawk.exe -tool-mod` process the mod actually runs in is that same 32-bit binary. Pinning `@architecture x86-64` (a leftover from when the host was 64-bit `explorer.exe`) made Windhawk never load the mod into the 32-bit `windhawk.exe` — silent no-op, no log, no toast. So the mod **must** build for x86; the restriction was removed (matching working tool mods like `theme-toggler-tray`).
- `Insert` reports the OS toggle bit, not any app's overtype mode; it is off by default.
- Design spec and implementation plan are under `docs/superpowers/`. Update the spec when making a conscious deviation from it.
- Per the user's global preference: keep commit subjects short; never push without being asked.
