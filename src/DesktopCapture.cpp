#include "DesktopCapture.h"

using Microsoft::WRL::ComPtr;

HRESULT DesktopCapture::Init(ID3D11Device* device, IDXGIOutput* output) {
    Shutdown();

    ComPtr<IDXGIOutput1> output1;
    HRESULT hr = output->QueryInterface(IID_PPV_ARGS(&output1));
    if (FAILED(hr)) return hr;

    hr = output1->DuplicateOutput(device, &duplication_);
    if (FAILED(hr)) {
        duplication_.Reset();
        return hr;
    }

    DXGI_OUTDUPL_DESC desc{};
    duplication_->GetDesc(&desc);
    width_ = desc.ModeDesc.Width;
    height_ = desc.ModeDesc.Height;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width_;
    td.Height = height_;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = desc.ModeDesc.Format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D(&td, nullptr, &copy_);
    if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(copy_.Get(), nullptr, &srv_);
    if (FAILED(hr)) {
        Shutdown();
        return hr;
    }

    device->GetImmediateContext(&context_);
    hasFrame_ = false;
    return S_OK;
}

void DesktopCapture::Shutdown() {
    srv_.Reset();
    copy_.Reset();
    context_.Reset();
    duplication_.Reset();
    width_ = height_ = 0;
    hasFrame_ = false;
}

DesktopCapture::Status DesktopCapture::Acquire(UINT timeoutMs) {
    if (!duplication_) return Status::Failed;

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    const HRESULT hr = duplication_->AcquireNextFrame(timeoutMs, &info, &resource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return Status::NoChange;
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_ACCESS_DENIED || hr == E_ACCESSDENIED) {
        return Status::AccessLost;
    }
    if (FAILED(hr)) return Status::Failed;

    Status result = Status::NoChange;
    // LastPresentTime == 0 means only the mouse pointer changed; the desktop image is unchanged.
    if (info.LastPresentTime.QuadPart != 0 || !hasFrame_) {
        ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(resource.As(&texture))) {
            context_->CopyResource(copy_.Get(), texture.Get());
            hasFrame_ = true;
            result = Status::NewFrame;
        }
        else {
            result = Status::Failed;
        }
    }
    duplication_->ReleaseFrame();
    return result;
}