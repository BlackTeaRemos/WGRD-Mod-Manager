#include "gui/design/interaction/InteractiveRegion.h"

namespace wgrd::gui::design {
bool InteractiveRegion::Activate(const ImVec2 topLeft, const ImVec2 bottomRight, bool& highlighted, const bool allowOverlap) {
	highlighted = false;
	if (bottomRight.x <= topLeft.x || bottomRight.y <= topLeft.y) {
		return false;
	}
	const ImVec2 previous = ImGui::GetCursorScreenPos();
	const ImVec2 origin = ImGui::GetWindowPos();
	ImGui::PushID(static_cast<int>(topLeft.x - origin.x + ImGui::GetScrollX()));
	ImGui::PushID(static_cast<int>(topLeft.y - origin.y + ImGui::GetScrollY()));
	ImGui::PushID(static_cast<int>(bottomRight.x - topLeft.x));
	ImGui::PushID(static_cast<int>(bottomRight.y - topLeft.y));
	ImGui::SetCursorScreenPos(topLeft);
	const bool activated = ImGui::InvisibleButton("##action",
		ImVec2(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y),
		ImGuiButtonFlags_EnableNav | (allowOverlap ? ImGuiButtonFlags_AllowOverlap : ImGuiButtonFlags_None));
	highlighted = ImGui::IsItemHovered() || ImGui::IsItemFocused();
	ImGui::SetCursorScreenPos(previous);
	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
	return activated;
}

bool InteractiveRegion::Secondary(const ImVec2 topLeft, const ImVec2 bottomRight) {
	const ImVec2 pointer = ImGui::GetIO().MousePos;
	const bool inside = pointer.x >= topLeft.x && pointer.x < bottomRight.x
		&& pointer.y >= topLeft.y && pointer.y < bottomRight.y;
	return inside && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
}
}
