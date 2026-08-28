#pragma once

#include "gui/render/IRenderBackend.h"

#include <memory>
#include <vector>

namespace wgrd::gui {
class RenderBackendProbe {
public:
	[[nodiscard]] static std::vector<RenderBackendKind> PreferredOrder();

	[[nodiscard]] static std::unique_ptr<IRenderBackend> Create(RenderBackendKind kind);

	[[nodiscard]] static std::unique_ptr<IRenderBackend> Select(HWND window);
};
}
