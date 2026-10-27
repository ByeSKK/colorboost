#include "Monitors.h"

#include <shellscalingapi.h>

#include "Utils.h"

using Microsoft::WRL::ComPtr;

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> result;

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return result;

    for (UINT a = 0;; ++a) {
        ComPtr<IDXGIAdapter1> adapter;
        if (FAILED(factory->EnumAdapters1(a, &adapter))) break;

        DXGI_ADAPTER_DESC1 adapterDesc{};
        adapter->GetDesc1(&adapterDesc);
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        for (UINT o = 0;; ++o) {
            ComPtr<IDXGIOutput> output;
            if (FAILED(adapter->EnumOutputs(o, &output))) break;

            DXGI_OUTPUT_DESC desc{};
            if (FAILED(output->GetDesc(&desc)) || !desc.AttachedToDesktop) continue;

            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(desc.Monitor, &mi);

            MonitorInfo info;
            info.name = util::WideToUtf8(desc.DeviceName);
            info.rect = desc.DesktopCoordinates;
            info.adapterIndex = a;
            info.outputIndex = o;
            info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

            UINT dpiX = 96, dpiY = 96;
            if (SUCCEEDED(GetDpiForMonitor(desc.Monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) info.dpi = dpiX;

            std::wstring label = std::to_wstring(result.size() + 1) + L": " + desc.DeviceName + L" (" +
                std::to_wstring(info.Width()) + L"x" + std::to_wstring(info.Height()) + L")";
            if (info.primary) label += L" [primary]";
            info.label = util::WideToUtf8(label);

            result.push_back(std::move(info));
        }
    }
    return result;
}

int FindMonitorIndex(const std::vector<MonitorInfo>& monitors, const std::string& name) {
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (!name.empty() && monitors[i].name == name) return static_cast<int>(i);
    }
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].primary) return static_cast<int>(i);
    }
    return 0;
}

bool OpenMonitorOutput(const MonitorInfo& monitor, ComPtr<IDXGIAdapter1>& adapter, ComPtr<IDXGIOutput>& output) {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
    if (FAILED(factory->EnumAdapters1(monitor.adapterIndex, &adapter))) return false;
    if (FAILED(adapter->EnumOutputs(monitor.outputIndex, &output))) return false;
    return true;
}