#include "gui/render/Direct3D11Backend.h"

#include <imgui_impl_dx11.h>
#include <imgui.h>

namespace wgrd::gui {

namespace {

constexpr float CLEAR_COLOR[4] = {0.027f, 0.027f, 0.039f, 1.0f};

}

Direct3D11Backend::~Direct3D11Backend() {
    Shutdown();
}

bool Direct3D11Backend::Initialize(HWND window) {
    if (!CreateDevice_(window)) {
        return false;
    }

    if (!ImGui_ImplDX11_Init(_device, _context)) {
        Shutdown();
        return false;
    }

    _imguiReady = true;
    return true;
}

void Direct3D11Backend::Shutdown() {
    if (_imguiReady) {
        ImGui_ImplDX11_Shutdown();
        _imguiReady = false;
    }

    ReleaseRenderTarget_();

    if (_swapChain != nullptr) {
        _swapChain->Release();
        _swapChain = nullptr;
    }
    if (_context != nullptr) {
        _context->Release();
        _context = nullptr;
    }
    if (_device != nullptr) {
        _device->Release();
        _device = nullptr;
    }
}

void Direct3D11Backend::BeginFrame() {
    ImGui_ImplDX11_NewFrame();
}

void Direct3D11Backend::Present(ImDrawData* drawData) {
    if (_renderTarget == nullptr) {
        CreateRenderTarget_();
    }

    _context->OMSetRenderTargets(1, &_renderTarget, nullptr);
    _context->ClearRenderTargetView(_renderTarget, CLEAR_COLOR);

    ImGui_ImplDX11_RenderDrawData(drawData);

    _swapChain->Present(1, 0);
}

void Direct3D11Backend::Resize(std::uint32_t width, std::uint32_t height) {
    if (_swapChain == nullptr || width == 0 || height == 0) {
        return;
    }

    ReleaseRenderTarget_();
    _swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget_();
}

RenderBackendKind Direct3D11Backend::Kind() const {
    return RenderBackendKind::Direct3D11;
}

std::string_view Direct3D11Backend::Name() const {
    return "dx11";
}

bool Direct3D11Backend::CreateDevice_(HWND window) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferDesc.RefreshRate.Numerator = 60;
    description.BufferDesc.RefreshRate.Denominator = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        requested,
        static_cast<UINT>(std::size(requested)),
        D3D11_SDK_VERSION,
        &description,
        &_swapChain,
        &_device,
        &obtained,
        &_context);

    if (FAILED(result)) {
        return false;
    }

    CreateRenderTarget_();
    return true;
}

void Direct3D11Backend::CreateRenderTarget_() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || backBuffer == nullptr) {
        return;
    }

    _device->CreateRenderTargetView(backBuffer, nullptr, &_renderTarget);
    backBuffer->Release();
}

void Direct3D11Backend::ReleaseRenderTarget_() {
    if (_renderTarget != nullptr) {
        _renderTarget->Release();
        _renderTarget = nullptr;
    }
}

}
