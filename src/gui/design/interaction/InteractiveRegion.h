#pragma once

#include <imgui.h>

namespace wgrd::gui::design {
class InteractiveRegion {
public:
	[[nodiscard]] static bool Activate(ImVec2 topLeft, ImVec2 bottomRight, bool& highlighted, bool allowOverlap);
	[[nodiscard]] static bool Secondary(ImVec2 topLeft, ImVec2 bottomRight);
};
}
