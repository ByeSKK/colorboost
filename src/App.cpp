#include "App.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "Utils.h"

// Declared in imgui_impl_win32.cpp (hidden behind #if 0 in the header).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

using Microsoft::WRL::ComPtr;

namespace {

    constexpr wchar_t kCtlClass[] = L"ColorBoost.Controller";
    constexpr wchar_t kOverlayClass[] = L"ColorBoost.Overlay";
    constexpr wchar_t kPanelClass[] = L"ColorBoost.Panel";
    constexpr UINT WM_TRAYICON = WM_APP + 1;
    constexpr UINT_PTR kModalTimerId = 1;

    enum HotkeyId : int { HK_PANEL = 1, HK_EFFECT = 2, HK_NEXT = 3, HK_RESET = 4 };

    App* g_app = nullptr;

    int64_t NowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    LRESULT CALLBACK CtlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return g_app ? g_app->OnCtlMessage(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return g_app ? g_app->OnPanelMessage(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // Colour-wheel icon generated at runtime so no .ico resource is needed.
    HICON CreateAppIcon() {
        constexpr int size = 32;
        std::vector<uint32_t> pixels(static_cast<size_t>(size) * size, 0);
        const float center = (size - 1) * 0.5f;
        const float radius = 14.5f;
        const float pi = 3.14159265f;

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float dx = x - center;
                const float dy = y - center;
                const float d = std::sqrt(dx * dx + dy * dy);
                const float coverage = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
                if (coverage <= 0.0f) continue;

                const float hue = (std::atan2(dy, dx) + pi) / (2.0f * pi);
                const float s = std::clamp(d / radius, 0.0f, 1.0f);
                const float v = 1.0f;
                const float h6 = hue * 6.0f;
                const int hi = static_cast<int>(h6) % 6;
                const float f = h6 - std::floor(h6);
                const float p = v * (1.0f - s);
                const float q = v * (1.0f - s * f);
                const float t = v * (1.0f - s * (1.0f - f));
                float r = v, g = t, b = p;
                switch (hi) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
                }
                const uint32_t a8 = static_cast<uint32_t>(coverage * 255.0f);
                const uint32_t r8 = static_cast<uint32_t>(r * 255.0f);
                const uint32_t g8 = static_cast<uint32_t>(g * 255.0f);
                const uint32_t b8 = static_cast<uint32_t>(b * 255.0f);
                pixels[static_cast<size_t>(y) * size + x] = (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
            }
        }

        HBITMAP color = CreateBitmap(size, size, 1, 32, pixels.data());
        HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
        HICON icon = nullptr;
        if (color && mask) {
            ICONINFO info{};
            info.fIcon = TRUE;
            info.hbmColor = color;
            info.hbmMask = mask;
            icon = CreateIconIndirect(&info);
        }
        if (color) DeleteObject(color);
        if (mask) DeleteObject(mask);
        return icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    }

}  // namespace

// ============================================================================
// Lifecycle
// ============================================================================

App::App() { g_app = this; }

App::~App() {
    BeginShutdown();
    ReleaseGraphics();
    if (win32Ready_) {
        ImGui_ImplWin32_Shutdown();
        win32Ready_ = false;
    }
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();

    if (overlay_) DestroyWindow(overlay_);
    if (panel_) DestroyWindow(panel_);
    if (ctl_) DestroyWindow(ctl_);
    overlay_ = panel_ = ctl_ = nullptr;

    if (hInstance_) {
        UnregisterClassW(kCtlClass, hInstance_);
        UnregisterClassW(kOverlayClass, hInstance_);
        UnregisterClassW(kPanelClass, hInstance_);
    }
    if (trayIcon_) DestroyIcon(trayIcon_);
    g_app = nullptr;
}

void App::ShowError(const std::string& utf8Message) {
    MessageBoxW(nullptr, util::Utf8ToWide(utf8Message).c_str(), L"ColorBoost", MB_OK | MB_ICONERROR);
}

bool App::Initialize(HINSTANCE hInstance, bool startHidden) {
    hInstance_ = hInstance;
    firstRun_ = !LoadConfig(cfg_);
    autostart_ = util::IsAutostartEnabled();

    monitors_ = EnumerateMonitors();
    if (monitors_.empty()) {
        ShowError("Не найден ни один активный монитор / No active monitor found.");
        return false;
    }
    activeMonitor_ = FindMonitorIndex(monitors_, cfg_.monitor);
    cfg_.monitor = monitors_[activeMonitor_].name;

    if (cfg_.uiScale <= 0.0f) {
        cfg_.uiScale = std::clamp(monitors_[activeMonitor_].dpi / 96.0f, 1.0f, 3.0f);
    }
    cfg_.uiScale = std::clamp(cfg_.uiScale, 0.75f, 3.0f);

    if (!RegisterWindowClasses() || !CreateWindows()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    if (!ImGui_ImplWin32_Init(panel_)) {
        ShowError("ImGui_ImplWin32_Init failed.");
        return false;
    }
    win32Ready_ = true;
    ApplyUiScale();

    CreateTrayIcon();
    RegisterHotkeys();

    if (!RebuildGraphics()) {
        ShowError("Не удалось инициализировать графику / Graphics initialisation failed:\n\n" + statusError_);
        return false;
    }

    if (!startHidden) ShowPanel(true);
    return true;
}

int App::Run() {
    MSG msg{};
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Tick(16, true);
    }
}

void App::BeginShutdown() {
    if (shutdownDone_) return;
    shutdownDone_ = true;
    if (panelVisible_) SaveWindowRect();
    SaveConfigIfNeeded(true);
    UnregisterHotkeys();
    RemoveTrayIcon();
}

// ============================================================================
// Windows
// ============================================================================

bool App::RegisterWindowClasses() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    wc.lpfnWndProc = CtlProc;
    wc.lpszClassName = kCtlClass;
    if (!RegisterClassExW(&wc)) return false;

    wc.lpfnWndProc = OverlayProc;
    wc.lpszClassName = kOverlayClass;
    if (!RegisterClassExW(&wc)) return false;

    wc.lpfnWndProc = PanelProc;
    wc.lpszClassName = kPanelClass;
    if (!RegisterClassExW(&wc)) return false;
    return true;
}

bool App::CreateWindows() {
    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");

    ctl_ = CreateWindowExW(WS_EX_TOOLWINDOW, kCtlClass, L"ColorBoost Controller", WS_POPUP, 0, 0, 0, 0, nullptr,
        nullptr, hInstance_, nullptr);
    if (!ctl_) {
        ShowError("Failed to create the controller window.");
        return false;
    }

    const MonitorInfo& mon = monitors_[activeMonitor_];

    overlay_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT, kOverlayClass,
        L"ColorBoost Overlay", WS_POPUP, mon.rect.left, mon.rect.top, mon.Width(), mon.Height(), nullptr, nullptr,
        hInstance_, nullptr);
    if (!overlay_) {
        ShowError("Failed to create the overlay window.");
        return false;
    }
    SetLayeredWindowAttributes(overlay_, 0, 255, LWA_ALPHA);

    // Settings panel: a regular resizable tool window that never takes focus from the game.
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    const float scale = cfg_.uiScale;

    RECT rc{ 0, 0, static_cast<LONG>(480 * scale), static_cast<LONG>(760 * scale) };
    rc.bottom = std::min<LONG>(rc.bottom, static_cast<LONG>(mon.Height() * 9 / 10));
    AdjustWindowRectExForDpi(&rc, style, FALSE, exStyle, mon.dpi);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int x = mon.rect.left + static_cast<int>(40 * scale);
    int y = mon.rect.top + static_cast<int>(40 * scale);

    if (cfg_.hasPanelRect && cfg_.panelW >= 240 && cfg_.panelH >= 240) {
        RECT saved{ cfg_.panelX, cfg_.panelY, cfg_.panelX + cfg_.panelW, cfg_.panelY + cfg_.panelH };
        if (MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            x = cfg_.panelX;
            y = cfg_.panelY;
            w = cfg_.panelW;
            h = cfg_.panelH;
        }
    }

    panel_ = CreateWindowExW(exStyle, kPanelClass, L"ColorBoost", style, x, y, w, h, nullptr, nullptr, hInstance_,
        nullptr);
    if (!panel_) {
        ShowError("Failed to create the panel window.");
        return false;
    }

    // Both windows must be excluded from capture, otherwise the effect would feed back into itself.
    if (!SetWindowDisplayAffinity(overlay_, WDA_EXCLUDEFROMCAPTURE) ||
        !SetWindowDisplayAffinity(panel_, WDA_EXCLUDEFROMCAPTURE)) {
        ShowError(
            "Требуется Windows 10 версии 2004 (сборка 19041) или новее.\n"
            "Windows 10 version 2004 (build 19041) or newer is required.");
        return false;
    }
    return true;
}

void App::CreateTrayIcon() {
    if (!ctl_) return;
    if (!trayIcon_) trayIcon_ = CreateAppIcon();

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ctl_;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = trayIcon_;
    wcscpy_s(nid.szTip, L"ColorBoost");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void App::RemoveTrayIcon() {
    if (!ctl_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ctl_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void App::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    const std::wstring show = util::Utf8ToWide(Tr(Str::TrayShow));
    const std::wstring effect = util::Utf8ToWide(Tr(Str::TrayEffect));
    const std::wstring exit = util::Utf8ToWide(Tr(Str::TrayExit));
    AppendMenuW(menu, MF_STRING, 1, show.c_str());
    AppendMenuW(menu, MF_STRING | (cfg_.effectEnabled ? MF_CHECKED : MF_UNCHECKED), 2, effect.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, exit.c_str());

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(ctl_);

    // The popup menu runs a modal loop; keep the overlay updating through a timer.
    inModalLoop_ = true;
    SetTimer(ctl_, kModalTimerId, 10, nullptr);
    const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, ctl_,
        nullptr);
    KillTimer(ctl_, kModalTimerId);
    inModalLoop_ = false;
    PostMessageW(ctl_, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (command) {
    case 1: TogglePanel(); break;
    case 2: ToggleEffect(); break;
    case 3: PostMessageW(ctl_, WM_CLOSE, 0, 0); break;
    default: break;
    }
}

void App::RegisterHotkeys() {
    hotkeyError_.clear();
    const UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    bool ok = true;
    ok &= RegisterHotKey(ctl_, HK_PANEL, mods, VK_F10) != FALSE;
    ok &= RegisterHotKey(ctl_, HK_EFFECT, mods, VK_F9) != FALSE;
    ok &= RegisterHotKey(ctl_, HK_NEXT, mods, VK_F11) != FALSE;
    ok &= RegisterHotKey(ctl_, HK_RESET, mods, VK_F8) != FALSE;
    if (!ok) hotkeyError_ = Tr(Str::HotkeyFail);
}

void App::UnregisterHotkeys() {
    if (!ctl_) return;
    for (int id : {HK_PANEL, HK_EFFECT, HK_NEXT, HK_RESET}) UnregisterHotKey(ctl_, id);
}

LRESULT App::OnCtlMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMsg_ != 0 && msg == taskbarCreatedMsg_) {
        CreateTrayIcon();
        return 0;
    }
    switch (msg) {
    case WM_HOTKEY:
        switch (wParam) {
        case HK_PANEL: TogglePanel(); break;
        case HK_EFFECT: ToggleEffect(); break;
        case HK_NEXT: CyclePreset(); break;
        case HK_RESET: ResetCurrent(); break;
        default: break;
        }
        return 0;

    case WM_TRAYICON:
        switch (LOWORD(lParam)) {
        case WM_LBUTTONUP: TogglePanel(); break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: ShowTrayMenu(); break;
        default: break;
        }
        return 0;

    case WM_DISPLAYCHANGE:
        needRebuild_ = true;
        nextRebuildAt_ = NowMs() + 500;
        return 0;

    case WM_TIMER:
        if (wParam == kModalTimerId) Tick(0, false);
        return 0;

    case WM_ENDSESSION:
        if (wParam) BeginShutdown();
        return 0;

    case WM_DESTROY:
        BeginShutdown();
        ctl_ = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::OnPanelMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;

    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && gfxReady_) {
            renderer_.ResizePanel(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = static_cast<LONG>(340 * appliedScale_);
        info->ptMinTrackSize.y = static_cast<LONG>(320 * appliedScale_);
        return 0;
    }

    case WM_ENTERSIZEMOVE:
        // The move/size loop is modal: keep the overlay alive through a timer.
        inModalLoop_ = true;
        SetTimer(hwnd, kModalTimerId, 10, nullptr);
        return 0;

    case WM_EXITSIZEMOVE:
        KillTimer(hwnd, kModalTimerId);
        inModalLoop_ = false;
        SaveWindowRect();
        MarkConfigDirty();
        return 0;

    case WM_TIMER:
        if (wParam == kModalTimerId) Tick(0, false);
        return 0;

    case WM_DPICHANGED: {
        const UINT dpi = HIWORD(wParam);
        cfg_.uiScale = std::clamp(dpi / 96.0f, 0.75f, 3.0f);
        fontsDirty_ = true;
        MarkConfigDirty();
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
            suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;

    case WM_CLOSE:
        ShowPanel(false);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void App::ShowPanel(bool show) {
    if (show) {
        if (!panelVisible_) {
            panelVisible_ = true;
            ShowWindow(panel_, SW_SHOWNOACTIVATE);
            lastPanelRenderAt_ = 0;
        }
        SetWindowPos(panel_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    else if (panelVisible_) {
        SaveWindowRect();
        panelVisible_ = false;
        ShowWindow(panel_, SW_HIDE);
        MarkConfigDirty();
    }
}

void App::SaveWindowRect() {
    RECT r{};
    if (panel_ && GetWindowRect(panel_, &r)) {
        cfg_.hasPanelRect = true;
        cfg_.panelX = r.left;
        cfg_.panelY = r.top;
        cfg_.panelW = r.right - r.left;
        cfg_.panelH = r.bottom - r.top;
    }
}

// ============================================================================
// Graphics
// ============================================================================

bool App::RebuildGraphics() {
    ReleaseGraphics();

    monitors_ = EnumerateMonitors();
    if (monitors_.empty()) {
        statusError_ = "No active monitors found.";
        return false;
    }
    activeMonitor_ = FindMonitorIndex(monitors_, cfg_.monitor);
    const MonitorInfo& mon = monitors_[activeMonitor_];

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    if (!OpenMonitorOutput(mon, adapter, output)) {
        statusError_ = "Could not open the selected monitor output.";
        return false;
    }

    std::string error;
    if (!renderer_.Init(adapter.Get(), error)) {
        statusError_ = error;
        return false;
    }

    SetWindowPos(overlay_, HWND_TOPMOST, mon.rect.left, mon.rect.top, mon.Width(), mon.Height(), SWP_NOACTIVATE);
    if (!renderer_.CreateOverlayTarget(overlay_, static_cast<UINT>(mon.Width()), static_cast<UINT>(mon.Height()))) {
        statusError_ = "Could not create the overlay swap chain.";
        renderer_.Shutdown();
        return false;
    }

    RECT client{};
    GetClientRect(panel_, &client);
    if (!renderer_.CreatePanelTarget(panel_, static_cast<UINT>(std::max<LONG>(client.right, 1)),
        static_cast<UINT>(std::max<LONG>(client.bottom, 1)))) {
        statusError_ = "Could not create the panel swap chain.";
        renderer_.Shutdown();
        return false;
    }

    if (!ImGui_ImplDX11_Init(renderer_.Device(), renderer_.Context())) {
        statusError_ = "ImGui_ImplDX11_Init failed.";
        renderer_.Shutdown();
        return false;
    }
    dxReady_ = true;
    gfxReady_ = true;
    overlayDirty_ = true;
    nextCaptureAt_ = 0;
    statusError_.clear();
    captureError_.clear();
    return true;
}

void App::ReleaseGraphics() {
    gfxReady_ = false;
    capture_.Shutdown();
    if (overlay_) ShowWindow(overlay_, SW_HIDE);
    overlayShown_ = false;
    if (dxReady_) {
        ImGui_ImplDX11_Shutdown();
        dxReady_ = false;
    }
    renderer_.Shutdown();
}

bool App::StartCapture() {
    if (monitors_.empty()) return false;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    if (!OpenMonitorOutput(monitors_[activeMonitor_], adapter, output)) {
        captureError_ = "Could not open the monitor output.";
        return false;
    }
    const HRESULT hr = capture_.Init(renderer_.Device(), output.Get());
    if (FAILED(hr)) {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "DuplicateOutput failed: 0x%08X", static_cast<unsigned>(hr));
        captureError_ = buffer;
        return false;
    }
    captureError_.clear();
    overlayDirty_ = true;
    return true;
}

void App::StopCapture() {
    ShowOverlay(false);
    capture_.Shutdown();
}

void App::ShowOverlay(bool show) {
    if (!overlay_ || show == overlayShown_) return;
    overlayShown_ = show;
    if (show) {
        ShowWindow(overlay_, SW_SHOWNOACTIVATE);
        RaiseWindows();
    }
    else {
        ShowWindow(overlay_, SW_HIDE);
    }
}

void App::RaiseWindows() {
    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (overlayShown_) SetWindowPos(overlay_, HWND_TOPMOST, 0, 0, 0, 0, flags);
    // The panel must always stay above the overlay.
    if (panelVisible_) SetWindowPos(panel_, HWND_TOPMOST, 0, 0, 0, 0, flags);
}

void App::ApplyUiScale() {
    const float scale = std::clamp(cfg_.uiScale, 0.75f, 3.0f);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const float pixels = std::round(17.0f * scale);
    bool loaded = false;
    wchar_t windowsDir[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windowsDir, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        const std::wstring path = std::wstring(windowsDir) + L"\\Fonts\\segoeui.ttf";
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            const std::string utf8 = util::WideToUtf8(path);
            loaded = io.Fonts->AddFontFromFileTTF(utf8.c_str(), pixels, nullptr,
                io.Fonts->GetGlyphRangesCyrillic()) != nullptr;
        }
    }
    if (!loaded) {
        ImFontConfig config;
        config.SizePixels = pixels;
        io.Fonts->AddFontDefault(&config);
    }

    // ---- Modern dark theme
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);

    // Rounding
    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 6.0f;

    // Padding
    style.WindowPadding = ImVec2(18.0f, 16.0f);
    style.FramePadding = ImVec2(12.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 14.0f;

    // Borders
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    // Colors
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.54f, 0.60f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.085f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.085f, 0.105f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.085f, 0.105f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.07f, 0.09f, 0.50f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.23f, 0.28f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.19f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.65f, 1.00f, 0.60f);
    c[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.31f, 0.65f, 1.00f, 0.60f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.23f, 0.28f, 0.50f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.31f, 0.65f, 1.00f, 0.60f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.31f, 0.65f, 1.00f, 0.90f);
    c[ImGuiCol_Tab] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.16f, 0.38f, 0.65f, 1.00f);
    c[ImGuiCol_TabSelectedOverline] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.13f, 0.25f, 0.42f, 1.00f);
    c[ImGuiCol_PlotLines] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_PlotLinesHovered] = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.31f, 0.65f, 1.00f, 0.35f);
    c[ImGuiCol_DragDropTarget] = ImVec4(0.31f, 0.65f, 1.00f, 0.90f);
    c[ImGuiCol_NavCursor] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);

    style.ScaleAllSizes(scale);
    ImGui::GetStyle() = style;

    appliedScale_ = scale;
    if (dxReady_) ImGui_ImplDX11_InvalidateDeviceObjects();
}

// ============================================================================
// Frame loop
// ============================================================================

void App::WaitForMessages(DWORD milliseconds) {
    MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}

void App::UpdateForegroundApp(int64_t now) {
    if (now - lastForegroundCheckAt_ < 250) return;
    lastForegroundCheckAt_ = now;
    const std::string exe = util::ForegroundProcessExe(GetCurrentProcessId());
    if (exe.empty() || exe == lastExe_) return;
    lastExe_ = exe;
    overlayDirty_ = true;  // parameters may differ (profile)
}

void App::Tick(UINT acquireTimeoutMs, bool allowIdleWait) {
    if (!ctl_) return;
    const int64_t now = NowMs();

    if (fontsDirty_) {  // never inside an ImGui frame
        ApplyUiScale();
        fontsDirty_ = false;
    }

    UpdateForegroundApp(now);
    SaveConfigIfNeeded(false);

    if (needRebuild_ && now >= nextRebuildAt_) {
        if (RebuildGraphics()) {
            needRebuild_ = false;
        }
        else {
            nextRebuildAt_ = now + 1000;
        }
    }
    if (!gfxReady_) {
        if (allowIdleWait) WaitForMessages(50);
        return;
    }

    bool waited = false;
    if (EffectActive()) {
        if (!capture_.Valid() && now >= nextCaptureAt_) {
            if (!StartCapture()) nextCaptureAt_ = now + 500;
        }
        if (capture_.Valid()) {
            const DesktopCapture::Status status = capture_.Acquire(acquireTimeoutMs);
            waited = acquireTimeoutMs > 0;
            switch (status) {
            case DesktopCapture::Status::NewFrame:
                overlayDirty_ = true;
                break;
            case DesktopCapture::Status::AccessLost:
                // Secure desktop (UAC), resolution change, GPU reset: hide the stale picture and retry.
                StopCapture();
                nextCaptureAt_ = now + 250;
                break;
            case DesktopCapture::Status::Failed:
                StopCapture();
                nextCaptureAt_ = now + 1000;
                break;
            case DesktopCapture::Status::NoChange:
                break;
            }
            if (capture_.Valid() && capture_.HasFrame() && overlayDirty_) RenderOverlay();
        }
    }
    else if (capture_.Valid() || overlayShown_) {
        StopCapture();
    }

    if (panelVisible_ && now - lastPanelRenderAt_ >= 15) {
        RenderPanel();
        lastPanelRenderAt_ = NowMs();
    }

    // Some games re-assert HWND_TOPMOST for themselves; take the top back periodically.
    if (!inModalLoop_ && now - lastRaiseAt_ >= 1000) {
        lastRaiseAt_ = now;
        RaiseWindows();
    }

    if (!waited && allowIdleWait) WaitForMessages(panelVisible_ ? 8 : 100);
}

void App::RenderOverlay() {
    const ShaderParams params = MakeShaderParams(CurrentParams(), cfg_.dither, frameCounter_++);
    const HRESULT hr = renderer_.DrawOverlay(capture_.Srv(), params);
    overlayDirty_ = false;

    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            needRebuild_ = true;
            nextRebuildAt_ = NowMs() + 500;
        }
        return;
    }
    if (!overlayShown_) ShowOverlay(true);  // show only once the first processed frame exists

    const int64_t now = NowMs();
    ++framesInWindow_;
    if (fpsWindowStart_ == 0) fpsWindowStart_ = now;
    if (now - fpsWindowStart_ >= 1000) {
        fps_ = framesInWindow_ * 1000.0f / static_cast<float>(now - fpsWindowStart_);
        framesInWindow_ = 0;
        fpsWindowStart_ = now;
    }
}

void App::RenderPanel() {
    if (!gfxReady_ || !dxReady_ || !panelVisible_ || IsIconic(panel_)) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawPanel();
    ImGui::Render();

    const float clear[4] = { 0.09f, 0.09f, 0.10f, 1.0f };
    renderer_.BeginPanelFrame(clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT hr = renderer_.PresentPanel();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        needRebuild_ = true;
        nextRebuildAt_ = NowMs() + 500;
    }
}

// ============================================================================
// Logic
// ============================================================================

AppProfile* App::FindProfile(const std::string& exe) {
    if (exe.empty()) return nullptr;
    for (auto& p : cfg_.profiles) {
        if (p.exe == exe) return &p;
    }
    return nullptr;
}

const AppProfile* App::FindProfile(const std::string& exe) const {
    if (exe.empty()) return nullptr;
    for (const auto& p : cfg_.profiles) {
        if (p.exe == exe) return &p;
    }
    return nullptr;
}

ColorParams& App::EditTarget() {
    if (AppProfile* profile = ActiveProfile()) return profile->params;
    return cfg_.global;
}

const ColorParams& App::CurrentParams() const {
    if (const AppProfile* profile = FindProfile(lastExe_)) return profile->params;
    return cfg_.global;
}

bool App::EffectActive() const {
    if (!cfg_.effectEnabled) return false;
    if (cfg_.onlyForProfiles && FindProfile(lastExe_) == nullptr) return false;
    return true;
}

const char* App::StatusText() const {
    if (!cfg_.effectEnabled) return Tr(Str::StatusOff);
    if (cfg_.onlyForProfiles && FindProfile(lastExe_) == nullptr) return Tr(Str::StatusWaiting);
    if (!capture_.Valid() || !overlayShown_) return Tr(Str::StatusStarting);
    return Tr(Str::StatusActive);
}

int App::PresetCount() const {
    return static_cast<int>(BuiltinPresets().size() + cfg_.userPresets.size());
}

std::string App::PresetName(int index) const {
    const auto& builtin = BuiltinPresets();
    const size_t i = static_cast<size_t>(index);
    if (i < builtin.size()) {
        return cfg_.language == Language::English ? builtin[i].nameEn : builtin[i].nameRu;
    }
    return cfg_.userPresets[i - builtin.size()].name;
}

const ColorParams& App::PresetParams(int index) const {
    const auto& builtin = BuiltinPresets();
    const size_t i = static_cast<size_t>(index);
    if (i < builtin.size()) return builtin[i].params;
    return cfg_.userPresets[i - builtin.size()].params;
}

void App::ApplyPreset(int index) {
    if (index < 0 || index >= PresetCount()) return;
    const ColorParams params = PresetParams(index);
    EditTarget() = params;
    selectedPreset_ = index;
    MarkParamsEdited();
}

void App::SaveCurrentAsNewPreset() {
    const std::string base = Tr(Str::NewPresetName);
    std::string name;
    for (int n = 1;; ++n) {
        name = base + " " + std::to_string(n);
        const bool exists = std::any_of(cfg_.userPresets.begin(), cfg_.userPresets.end(),
            [&](const UserPreset& p) { return p.name == name; });
        if (!exists) break;
    }
    UserPreset preset;
    preset.name = name;
    preset.params = EditTarget();
    cfg_.userPresets.push_back(std::move(preset));
    selectedPreset_ = PresetCount() - 1;
    MarkConfigDirty();
}

void App::CyclePreset() {
    const int count = PresetCount();
    if (count <= 0) return;
    ApplyPreset((selectedPreset_ + 1) % count);
}

void App::ResetCurrent() {
    EditTarget() = ColorParams{};
    selectedPreset_ = 0;
    MarkParamsEdited();
}

void App::ToggleEffect() {
    cfg_.effectEnabled = !cfg_.effectEnabled;
    MarkParamsEdited();
}

void App::MarkConfigDirty() {
    configDirty_ = true;
    lastConfigChangeAt_ = NowMs();
}

void App::MarkParamsEdited() {
    overlayDirty_ = true;
    MarkConfigDirty();
}

void App::SaveConfigIfNeeded(bool force) {
    if (!configDirty_) return;
    if (!force && NowMs() - lastConfigChangeAt_ < 800) return;
    if (SaveConfig(cfg_)) configDirty_ = false;
}