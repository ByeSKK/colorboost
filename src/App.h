#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "DesktopCapture.h"
#include "Localization.h"
#include "Monitors.h"
#include "Renderer.h"
#include "Settings.h"

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Initialize(HINSTANCE hInstance, bool startHidden);
    int Run();

    // Window procedures (called from the static thunks).
    LRESULT OnCtlMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT OnPanelMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // ---- windows / system integration
    bool RegisterWindowClasses();
    bool CreateWindows();
    void CreateTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void BeginShutdown();
    void ShowError(const std::string& utf8Message);

    // ---- graphics
    bool RebuildGraphics();
    void ReleaseGraphics();
    bool StartCapture();
    void StopCapture();
    void ShowOverlay(bool show);
    void RaiseWindows();
    void ApplyUiScale();

    // ---- frame loop
    void Tick(UINT acquireTimeoutMs, bool allowIdleWait);
    void RenderOverlay();
    void RenderPanel();
    void UpdateForegroundApp(int64_t now);
    void WaitForMessages(DWORD milliseconds);

    // ---- user interface (AppUI.cpp)
    void DrawPanel();
    void DrawColorTab();
    void DrawProfilesTab();
    void DrawSettingsTab();
    bool ParamSlider(const char* id, Str label, float& value, float minValue, float maxValue,
        const char* format, float defaultValue);

    // ---- logic
    const char* Tr(Str id) const { return Loc(cfg_.language, id); }
    AppProfile* FindProfile(const std::string& exe);
    const AppProfile* FindProfile(const std::string& exe) const;
    AppProfile* ActiveProfile() { return FindProfile(lastExe_); }
    ColorParams& EditTarget();
    const ColorParams& CurrentParams() const;
    bool EffectActive() const;
    const char* StatusText() const;

    int PresetCount() const;
    std::string PresetName(int index) const;
    const ColorParams& PresetParams(int index) const;
    void ApplyPreset(int index);
    void SaveCurrentAsNewPreset();
    void CyclePreset();
    void ResetCurrent();
    void ToggleEffect();
    void TogglePanel() { ShowPanel(!panelVisible_); }
    void ShowPanel(bool show);
    void SaveWindowRect();
    void MarkConfigDirty();
    void MarkParamsEdited();
    void SaveConfigIfNeeded(bool force);

    // ---- state
    HINSTANCE hInstance_ = nullptr;
    HWND ctl_ = nullptr;      // hidden controller window (tray, hotkeys)
    HWND overlay_ = nullptr;  // full-screen click-through window with the processed picture
    HWND panel_ = nullptr;    // settings panel
    HICON trayIcon_ = nullptr;
    UINT taskbarCreatedMsg_ = 0;

    Config cfg_;
    bool firstRun_ = false;
    bool autostart_ = false;

    std::vector<MonitorInfo> monitors_;
    int activeMonitor_ = 0;

    Renderer renderer_;
    DesktopCapture capture_;

    bool win32Ready_ = false;
    bool dxReady_ = false;
    bool gfxReady_ = false;
    bool needRebuild_ = false;
    bool overlayShown_ = false;
    bool overlayDirty_ = false;
    bool panelVisible_ = false;
    bool fontsDirty_ = false;
    bool inModalLoop_ = false;
    bool shutdownDone_ = false;
    bool configDirty_ = false;
    float appliedScale_ = 1.0f;

    std::string lastExe_;
    std::string lastEditKey_;
    std::string statusError_;
    std::string captureError_;
    std::string hotkeyError_;
    int selectedPreset_ = -1;

    int64_t nextRebuildAt_ = 0;
    int64_t nextCaptureAt_ = 0;
    int64_t lastForegroundCheckAt_ = 0;
    int64_t lastPanelRenderAt_ = 0;
    int64_t lastRaiseAt_ = 0;
    int64_t lastConfigChangeAt_ = 0;
    int64_t fpsWindowStart_ = 0;
    int framesInWindow_ = 0;
    float fps_ = 0.0f;
    unsigned frameCounter_ = 0;
};