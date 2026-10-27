#pragma once
#include <windows.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <string>
#include <vector>

struct MonitorInfo {
    std::string name;   // DXGI device name, UTF-8 (\\.\DISPLAY1)
    std::string label;  // text for the UI
    RECT rect{};        // position and size in virtual desktop pixels
    UINT adapterIndex = 0;
    UINT outputIndex = 0;
    UINT dpi = 96;
    bool primary = false;

    int Width() const { return rect.right - rect.left; }
    int Height() const { return rect.bottom - rect.top; }
};

std::vector<MonitorInfo> EnumerateMonitors();

// Index of the monitor by name; falls back to the primary monitor, then to 0.
int FindMonitorIndex(const std::vector<MonitorInfo>& monitors, const std::string& name);

bool OpenMonitorOutput(const MonitorInfo& monitor,
    Microsoft::WRL::ComPtr<IDXGIAdapter1>& adapter,
    Microsoft::WRL::ComPtr<IDXGIOutput>& output);