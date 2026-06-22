// ==WindhawkMod==
// @id              lock-keys-notifier
// @name            Lock Keys Notifier
// @description     Shows a customizable toast when a lock key (Caps/Num/Scroll/Insert) is toggled
// @version         1.0.0
// @author          Havrlisan
// @github          https://github.com/havrlisan
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lgdiplus -ldwmapi -lwinmm -lgdi32 -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Lock Keys Notifier

Shows a small, customizable toast notification whenever a lock key
(Caps Lock, Num Lock, Scroll Lock, or Insert) is toggled, displaying its new
ON/OFF state. See the project README for full details.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <cstdint>

BOOL Wh_ModInit() {
    Wh_Log(L"Lock Keys Notifier init");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Lock Keys Notifier uninit");
}
