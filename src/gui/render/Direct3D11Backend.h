#pragma once

#include "gui/render/IRenderBackend.h"

#include <d3d11.h>

namespace wgrd::gui {

class Direct3D11Backend final : public IRenderBackend {
public:
    Direct3D11Backend() = default;
    ~Direct3D11Backend() override;

    Direct3D11Backend(const Direct3D11Backend&) = delete;
    Direct3D11Backend& operator=(const Direct3D11Backend&) = delete;

    [[nodiscard]] bool Initialize(HWND window) override;
    void Shutdown() override;

    void BeginFrame() override;
    void Present(ImDrawData* drawData) override;
    void Resize(std::uint32_t width, std::uint32_t height) override;

    [[nodiscard]] RenderBackendKind Kind() const override;
    [[nodiscard]] std::string_view Name() const override;

private:
    bool CreateDevice_(HWND window);
    void CreateRenderTarget_();
    void ReleaseRenderTarget_();

    ID3D11Device* _device = nullptr;
    ID3D11DeviceContext* _context = nullptr;
    IDXGISwapChain* _swapChain = nullptr;
    ID3D11RenderTargetView* _renderTarget = nullptr;
    bool _imguiReady = false;
};

}
