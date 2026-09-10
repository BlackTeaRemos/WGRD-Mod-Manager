#include "gui/state/KeyboardNavigation.h"

#include <imgui.h>
#include <array>

namespace wgrd::gui {
void KeyboardNavigation::Apply(ApplicationState& state) {
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
		return;
	}
	const ImGuiIO& input = ImGui::GetIO();
	if (!input.KeyCtrl || input.WantTextInput) {
		return;
	}
	constexpr std::array<ImGuiKey, 5> KEYS = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5};
	for (std::size_t position = 0; position < KEYS.size(); ++position) {
		if (ImGui::IsKeyPressed(KEYS[position], false)) {
			state.SetScreen(static_cast<Screen>(position));
		}
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Comma, false)) {
		state.OpenSettings();
	}
}
}
