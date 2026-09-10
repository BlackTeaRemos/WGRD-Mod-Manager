#include "gui/screens/layout/ScreenViewport.h"

#include <algorithm>

namespace wgrd::gui {
ScreenArea ScreenViewport::Begin(const ScreenArea& area, const Screen screen) {
	ImGui::SetCursorScreenPos(area.origin);
	ImGui::PushID(static_cast<int>(screen));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("content", ImVec2(std::max(area.width, 1.0f), std::max(area.height, 1.0f)),
		ImGuiChildFlags_None, ImGuiWindowFlags_NoSavedSettings);
	const ImVec2 available = ImGui::GetContentRegionAvail();
	const float minimumWidth = screen == Screen::Catalog ? 300.0f : (screen == Screen::Order || screen == Screen::Profiles ? 600.0f : 900.0f);
	return ScreenArea{ImGui::GetCursorScreenPos(), std::max(available.x, minimumWidth), available.y};
}

void ScreenViewport::End() {
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopID();
}
}