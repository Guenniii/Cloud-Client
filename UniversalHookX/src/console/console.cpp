#include <Windows.h>
#include "console.hpp"
#include "../utils/lifecycle/lifecycle.hpp"

static bool ownsConsole = false;
static bool redirectedInput = false;
static bool redirectedOutput = false;

void Console::Alloc() {
#ifndef DISABLE_LOGGING_CONSOLE
    // Repeated initialization must not lose ownership of our console.
    if (ownsConsole) return;
    ownsConsole = AllocConsole() != FALSE;
    if (!ownsConsole) return;

    SetConsoleTitleA("UniversalHookX - Debug Console");
    FILE* stream = nullptr;
    redirectedInput = freopen_s(&stream, "CONIN$", "r", stdin) == 0;
    redirectedOutput = freopen_s(&stream, "CONOUT$", "w", stdout) == 0;
    ShowWindow(GetConsoleWindow(), SW_SHOW);
#endif
}

void Console::Free() {
#ifndef DISABLE_LOGGING_CONSOLE
    HWND window = GetConsoleWindow();
    DWORD process = 0;
    const DWORD attachedCount = GetConsoleProcessList(&process, 1);
    const bool exclusive = attachedCount == 1 && process == GetCurrentProcessId();

    // Close our CONIN$/CONOUT$ handles without closing the CRT standard streams.
    // Leave inherited streams alone; other game components may still use them.
    FILE* stream = nullptr;
    if (redirectedOutput) {
        fflush(stdout);
        freopen_s(&stream, "NUL", "w", stdout);
        redirectedOutput = false;
    }
    if (redirectedInput) {
        freopen_s(&stream, "NUL", "r", stdin);
        redirectedInput = false;
    }

    // Never send WM_CLOSE: console close events can terminate attached processes.
    // Only hide a window belonging exclusively to this process, not a shared shell.
    if (window && exclusive) ShowWindow(window, SW_HIDE);
    if (FreeConsole()) {
        ownsConsole = false;
        Lifecycle::Trace("Console: detached successfully");
    } else {
        Lifecycle::Trace("Console: detach failed or no console attached");
    }
#endif
}
