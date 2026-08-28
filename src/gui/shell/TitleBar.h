#pragma once

#include "gui/platform/Win32Window.h"
#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <imgui.h>

#include <string>

namespace wgrd::gui {
class TitleBar {
public:
	void Draw(
		ImVec2 origin,
		float width,
		ApplicationState& state,
		Win32Window& window,
		const ApplicationServices& services
	);

private:
	[[nodiscard]] static std::string FormatRates_(const ApplicationServices& services);

	bool _dragging = false;
	ImVec2 _dragOrigin{};
};
}
