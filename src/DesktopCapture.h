#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// Wraps DXGI Desktop Duplication. Every new desktop frame is copied into its own
// GPU texture, so the picture stays valid between desktop updates.
class DesktopCapture {
public:
    enum class Status { NewFrame, NoChange, AccessLost, Failed };

    DesktopCapture() = default;
    ~DesktopCapture() { Shutdown(); }
    DesktopCapture(const DesktopCapture&) = delete;
    DesktopCapture& operator=(const DesktopCapture&) = delete;

    HRESULT Init(ID3D11Device* device, IDXGIOutput* output);
    void Shutdown();
    Status Acquire(UINT timeoutMs);

    bool Valid() const { return duplication_ != nullptr; }
    bool HasFrame() const { return hasFrame_; }
    ID3D11ShaderResourceView* Srv() const { return srv_.Get(); }
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

private:
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    UINT width_ = 0;
    UINT height_ = 0;
    bool hasFrame_ = false;
};