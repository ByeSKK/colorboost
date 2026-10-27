#include "Renderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace {

    const char kShaderSource[] = R"HLSL(
cbuffer Params : register(b0)
{
    float pInvGamma;
    float pBrightness;
    float pContrast;
    float pTemperature;
    float pShadows;
    float pHighlights;
    float pSatFactor;
    float pVibrance;
    float pDither;
    float pFrame;
    float pPad0;
    float pPad1;
};

Texture2D<float4> gDesktop : register(t0);

static const float3 kLuma = float3(0.2126, 0.7152, 0.0722);

struct VSOut
{
    float4 pos : SV_Position;
};

VSOut VSMain(uint id : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float3 SrgbToLinear(float3 c)
{
    float3 lo = c / 12.92;
    float3 hi = pow(max((c + 0.055) / 1.055, 0.0), 2.4);
    return lerp(hi, lo, step(c, 0.04045));
}

float3 LinearToSrgb(float3 c)
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(hi, lo, step(c, 0.0031308));
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float4 PSMain(VSOut input) : SV_Target
{
    int2 px = int2(input.pos.xy);
    float3 c = gDesktop.Load(int3(px, 0)).rgb;   // sRGB-encoded

    // 1. Temperature: white-point shift in linear light, luminance-preserving.
    if (abs(pTemperature) > 0.0001)
    {
        float3 lin = SrgbToLinear(c);
        float3 m = float3(1.0 + 0.28 * pTemperature, 1.0 + 0.05 * pTemperature, 1.0 - 0.28 * pTemperature);
        m /= dot(m, kLuma);
        c = LinearToSrgb(saturate(lin * m));
    }

    // 2. Brightness (offset) and contrast (scale around mid-grey).
    c += pBrightness;
    c = (c - 0.5) * pContrast + 0.5;
    c = saturate(c);

    // 3. Shadows / highlights with luminance-based weights.
    float luma = dot(c, kLuma);
    float sw = 1.0 - smoothstep(0.0, 0.6, luma);
    float hw = smoothstep(0.4, 1.0, luma);
    if (pShadows >= 0.0)
        c += (1.0 - c) * (pShadows * sw * 0.4);
    else
        c *= 1.0 + pShadows * sw * 0.8;
    if (pHighlights >= 0.0)
        c += (1.0 - c) * (pHighlights * hw * 0.35);
    else
        c *= 1.0 + pHighlights * hw * 0.5;
    c = saturate(c);

    // 4. Gamma.
    c = pow(max(c, 0.0), pInvGamma);

    // 5. Saturation + vibrance (vibrance mostly affects muted colours).
    float gray = dot(c, kLuma);
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float chroma = (mx - mn) / max(mx, 1e-4);
    float vib = (pVibrance >= 0.0) ? pVibrance * (1.0 - chroma) : pVibrance * chroma;
    float factor = max(pSatFactor + vib * 1.5, 0.0);
    c = gray + (c - gray) * factor;

    // 6. Triangular dithering so stretched gradients do not band in 8 bit.
    if (pDither > 0.0)
    {
        float2 seed = input.pos.xy + pFrame * float2(37.0, 17.0);
        float n = Hash12(seed) + Hash12(seed + 19.19) - 1.0;
        c += n * pDither;
    }

    return float4(saturate(c), 1.0);
}
)HLSL";

    std::string HrToString(HRESULT hr) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned>(hr));
        return buffer;
    }

    std::string BlobText(ID3DBlob* blob) {
        if (!blob || blob->GetBufferSize() == 0) return {};
        return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
    }

}  // namespace

ShaderParams MakeShaderParams(const ColorParams& p, bool dither, unsigned frameCounter) {
    ShaderParams s{};
    s.invGamma = 1.0f / std::max(p.gamma, 0.05f);
    s.brightness = p.brightness / 100.0f * 0.30f;
    const float c = p.contrast / 100.0f;
    s.contrast = c >= 0.0f ? 1.0f + c * 1.5f : 1.0f + c;
    s.temperature = p.temperature / 100.0f;
    s.shadows = p.shadows / 100.0f;
    s.highlights = p.highlights / 100.0f;
    s.satFactor = 1.0f + p.saturation / 100.0f;
    s.vibrance = p.vibrance / 100.0f;
    s.dither = dither ? (0.75f / 255.0f) : 0.0f;
    s.frame = static_cast<float>(frameCounter & 1023u);
    return s;
}

bool Renderer::Init(IDXGIAdapter1* adapter, std::string& error) {
    Shutdown();

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL obtained{};
    HRESULT hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, D3D11_SDK_VERSION, &device_,
        &obtained, &context_);
    if (FAILED(hr)) {
        error = "D3D11CreateDevice failed: " + HrToString(hr);
        Shutdown();
        return false;
    }

    ComPtr<IDXGIDevice1> dxgiDevice;
    if (SUCCEEDED(device_.As(&dxgiDevice))) {
        dxgiDevice->SetMaximumFrameLatency(1);
        ComPtr<IDXGIAdapter> parent;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&parent))) parent->GetParent(IID_PPV_ARGS(&factory_));
    }
    if (!factory_) {
        error = "Could not obtain the DXGI factory.";
        Shutdown();
        return false;
    }

    if (!CreatePipeline(error)) {
        Shutdown();
        return false;
    }
    return true;
}

bool Renderer::CreatePipeline(std::string& error) {
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    ComPtr<ID3DBlob> vsBlob, psBlob, errors;

    HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "ColorBoost.hlsl", nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0, &vsBlob, &errors);
    if (FAILED(hr)) {
        error = "Vertex shader compile error: " + BlobText(errors.Get());
        return false;
    }
    errors.Reset();
    hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "ColorBoost.hlsl", nullptr, nullptr, "PSMain",
        "ps_5_0", flags, 0, &psBlob, &errors);
    if (FAILED(hr)) {
        error = "Pixel shader compile error: " + BlobText(errors.Get());
        return false;
    }

    hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_);
    if (FAILED(hr)) {
        error = "CreateVertexShader failed: " + HrToString(hr);
        return false;
    }
    hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_);
    if (FAILED(hr)) {
        error = "CreatePixelShader failed: " + HrToString(hr);
        return false;
    }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(ShaderParams);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device_->CreateBuffer(&bd, nullptr, &constantBuffer_);
    if (FAILED(hr)) {
        error = "CreateBuffer failed: " + HrToString(hr);
        return false;
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    hr = device_->CreateRasterizerState(&rd, &rasterizer_);
    if (FAILED(hr)) {
        error = "CreateRasterizerState failed: " + HrToString(hr);
        return false;
    }
    return true;
}

void Renderer::Shutdown() {
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }
    overlay_ = Target{};
    panel_ = Target{};
    rasterizer_.Reset();
    constantBuffer_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    factory_.Reset();
    context_.Reset();
    device_.Reset();
}

bool Renderer::CreateRtv(Target& target) {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(target.swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    return SUCCEEDED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &target.rtv));
}

bool Renderer::CreateTarget(Target& target, HWND hwnd, UINT width, UINT height) {
    if (!factory_ || !device_ || width == 0 || height == 0) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    target = Target{};
    if (FAILED(factory_->CreateSwapChainForHwnd(device_.Get(), hwnd, &desc, nullptr, nullptr, &target.swapChain))) {
        return false;
    }
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    target.width = width;
    target.height = height;
    return CreateRtv(target);
}

bool Renderer::CreateOverlayTarget(HWND hwnd, UINT width, UINT height) {
    return CreateTarget(overlay_, hwnd, width, height);
}

bool Renderer::CreatePanelTarget(HWND hwnd, UINT width, UINT height) {
    return CreateTarget(panel_, hwnd, width, height);
}

bool Renderer::ResizePanel(UINT width, UINT height) {
    if (!panel_.swapChain || width == 0 || height == 0) return true;
    if (width == panel_.width && height == panel_.height) return true;

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    panel_.rtv.Reset();
    if (FAILED(panel_.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
    panel_.width = width;
    panel_.height = height;
    return CreateRtv(panel_);
}

HRESULT Renderer::DrawOverlay(ID3D11ShaderResourceView* desktop, const ShaderParams& params) {
    if (!overlay_.swapChain || !overlay_.rtv || !desktop) return E_FAIL;

    context_->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &params, 0, 0);

    ID3D11RenderTargetView* rtv = overlay_.rtv.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(overlay_.width);
    viewport.Height = static_cast<float>(overlay_.height);
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
    context_->RSSetState(rasterizer_.Get());
    context_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);

    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    ID3D11Buffer* cb = constantBuffer_.Get();
    context_->PSSetConstantBuffers(0, 1, &cb);
    context_->PSSetShaderResources(0, 1, &desktop);

    context_->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv[1] = { nullptr };
    context_->PSSetShaderResources(0, 1, nullSrv);
    ID3D11RenderTargetView* nullRtv[1] = { nullptr };
    context_->OMSetRenderTargets(1, nullRtv, nullptr);

    return overlay_.swapChain->Present(0, 0);
}

void Renderer::BeginPanelFrame(const float clearColor[4]) {
    if (!panel_.rtv) return;
    ID3D11RenderTargetView* rtv = panel_.rtv.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    context_->ClearRenderTargetView(rtv, clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(panel_.width);
    viewport.Height = static_cast<float>(panel_.height);
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
}

HRESULT Renderer::PresentPanel() {
    if (!panel_.swapChain) return E_FAIL;
    const HRESULT hr = panel_.swapChain->Present(0, 0);
    ID3D11RenderTargetView* nullRtv[1] = { nullptr };
    context_->OMSetRenderTargets(1, nullRtv, nullptr);
    return hr;
}