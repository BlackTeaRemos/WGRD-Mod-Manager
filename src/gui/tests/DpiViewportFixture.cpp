#include "gui/tests/DpiViewportFixture.h"

namespace wgrd::gui::tests {
DpiViewportFixture::DpiViewportFixture()
	: _context(ImGui::CreateContext()) {
	_viewport.Initialize();
}

DpiViewportFixture::~DpiViewportFixture() {
	_viewport.Shutdown();
	ImGui::DestroyContext(_context);
}

DpiViewport& DpiViewportFixture::Viewport() {
	return _viewport;
}
}
