#include "gui/design/layout/CanvasExtent.h"

#include <imgui_internal.h>

#include <algorithm>

namespace wgrd::gui::design {
void CanvasExtent::Include(const ImVec2 bottomRight) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, bottomRight.x);
	window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, bottomRight.y);
}
}
