#include <windows.h>

#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR commandLine, int) {
    // Per-monitor DPI v2: pixel-exact coordinates for the overlay on any monitor.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ColorBoost.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
            L"ColorBoost уже запущен (значок в трее).\nColorBoost is already running (see the tray icon).",
            L"ColorBoost", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex);
        return 0;
    }

    const bool startHidden = commandLine && wcsstr(commandLine, L"--minimized") != nullptr;

    int exitCode = 1;
    {
        App app;
        if (app.Initialize(hInstance, startHidden)) exitCode = app.Run();
    }

    if (mutex) CloseHandle(mutex);
    return exitCode;
}