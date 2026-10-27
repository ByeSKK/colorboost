#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <string>

#include "Settings.h"

// Must match the cbuffer layout in the shader (12 floats = 3 registers).
struct ShaderParams {
    float invGamma;
    float brightness;
    float contrast;
    float temperature;
    float shadows;
    float highlights;
    float satFactor;
    float vibrance;
    float dither;
    float frame;
    float pad0;
    float pad1;
};
static_assert(sizeof(ShaderParams) % 16 == 0, "constant buffer size must be a multiple of 16");

ShaderParams MakeShaderParams(const ColorParams& params, bool dither, unsigned frameCounter);

class Renderer {
public:
    Renderer() = default;
    ~Renderer() { Shutdown(); }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init(IDXGIAdapter1* adapter, std::string& error);
    void Shutdown();

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }

    bool CreateOverlayTarget(HWND hwnd, UINT width, UINT height);
    bool CreatePanelTarget(HWND hwnd, UINT width, UINT height);
    bool ResizePanel(UINT width, UINT height);

    // Draws the desktop texture through the colour shader into the overlay window and presents it.
    HRESULT DrawOverlay(ID3D11ShaderResourceView* desktop, const ShaderParams& params);

    // Panel: BeginPanelFrame -> ImGui_ImplDX11_RenderDrawData -> PresentPanel.
    void BeginPanelFrame(const float clearColor[4]);
    HRESULT PresentPanel();

private:
    struct Target {
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        UINT width = 0;
        UINT height = 0;
    };

    bool CreateTarget(Target& target, HWND hwnd, UINT width, UINT height);
    bool CreateRtv(Target& target);
    bool CreatePipeline(std::string& error);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Target overlay_;
    Target panel_;
};