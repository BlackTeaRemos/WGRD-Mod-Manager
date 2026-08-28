#include "gui/render/RenderBackendProbe.h"

#include "gui/platform/WineDetection.h"
#include "gui/render/Direct3D11Backend.h"

namespace wgrd::gui {

std::vector<RenderBackendKind> RenderBackendProbe::PreferredOrder() {
    if (RunningUnderWine()) {
        return {
            RenderBackendKind::Vulkan,
            RenderBackendKind::OpenGL3,
            RenderBackendKind::Direct3D11
        };
    }

    return {
        RenderBackendKind::Direct3D11,
        RenderBackendKind::Direct3D12,
        RenderBackendKind::Vulkan,
        RenderBackendKind::OpenGL3
    };
}

std::unique_ptr<IRenderBackend> RenderBackendProbe::Create(RenderBackendKind kind) {
    switch (kind) {
        case RenderBackendKind::Direct3D11:
            return std::make_unique<Direct3D11Backend>();
        case RenderBackendKind::Direct3D12:
        case RenderBackendKind::Vulkan:
        case RenderBackendKind::OpenGL3:
            return nullptr;
    }

    return nullptr;
}

std::unique_ptr<IRenderBackend> RenderBackendProbe::Select(HWND window) {
    for (const RenderBackendKind kind : PreferredOrder()) {
        std::unique_ptr<IRenderBackend> backend = Create(kind);
        if (backend == nullptr) {
            continue;
        }
        if (backend->Initialize(window)) {
            return backend;
        }
    }

    return nullptr;
}

}
