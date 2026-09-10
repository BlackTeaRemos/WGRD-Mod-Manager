#include "gui/settings/GameLocationControls.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/Tokens.h"

#include <imgui.h>

namespace wgrd::gui {
void GameLocationControls::Draw(ApplicationState& state, const ApplicationServices& services) {
	const bool installing = services.install != nullptr && services.install->Progress().Busy();
	const bool publishing = services.publish != nullptr && services.publish->Progress().Busy();
	const bool updating = services.updates != nullptr && services.updates->Status().Busy();
	const bool available = services.files != nullptr && services.gameLocation != nullptr;
	const bool enabled = available && !installing && !publishing && !updating;
	ImGui::PushFont(design::FontLibrary::For(FontRole::Label), 11.0f);
	ImGui::BeginDisabled(!enabled);
	if (ImGui::Button("Choose game folder")) {
		const auto chosen = services.files->PickFolder("Choose game folder");
		if (chosen.has_value()) {
			const auto selected = services.gameLocation->SelectRoot(*chosen);
			if (selected.has_value()) {
				_notice.clear();
				state.RequestExit();
			} else {
				_notice = "Game folder invalid";
			}
		}
	}
	ImGui::EndDisabled();
	if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", available ? "Finish current operation" : "Folder selection unavailable");
	}
	if (!_notice.empty()) {
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(tokens::FAILURE), "%s", _notice.c_str());
	}
	ImGui::PopFont();
}
}
