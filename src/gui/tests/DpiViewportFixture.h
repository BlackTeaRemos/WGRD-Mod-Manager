#pragma once

#include "gui/platform/window/dpi/DpiViewport.h"

namespace wgrd::gui::tests {
class DpiViewportFixture {
public:
	DpiViewportFixture();
	~DpiViewportFixture();
	[[nodiscard]] DpiViewport& Viewport();

private:
	ImGuiContext* _context;
	DpiViewport _viewport;
};
}
