#include "gui/modals/settings/SettingsPanel.h"
#include "gui/modals/ModalSection.h"

#include "gui/modals/settings/TransferSettings.h"
#include "gui/modals/settings/TrustSettings.h"
#include "gui/modals/settings/UpdateSettings.h"
#include "gui/design/Tokens.h"

#include <imgui.h>

namespace wgrd::gui {
void SettingsPanel::Draw(ApplicationState& state, const ApplicationServices& services) {
	ModalSection::Draw("Game location");
	const bool located = services.order != nullptr && services.order->Current().located;
	if (located) {
		const std::string& root = services.order->Current().gameRoot;
		ImGui::TextWrapped("%s", root.c_str());
		if (ImGui::Button("Copy path")) {
			ImGui::SetClipboardText(root.c_str());
		}
		if (services.paths != nullptr) {
			ImGui::SameLine();
			if (ImGui::Button("Open folder")) {
				_notice = services.paths->OpenFolder(root).has_value() ? "" : "Folder could not open";
			}
		}
	}
	_location.Draw(state, services);
	if (!_notice.empty()) {
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(tokens::FAILURE), "%s", _notice.c_str());
	}
	ModalSection::Draw("Window");
	bool tray = state.BackgroundOnClose();
	ImGui::BeginDisabled(services.windowPreferences == nullptr);
	if (ImGui::Checkbox("Minimize to tray", &tray)) {
		state.SetBackgroundOnClose(tray);
		if (services.windowPreferences != nullptr) {
			auto preferences = services.windowPreferences->Load();
			preferences.backgroundOnClose = tray;
			_notice = services.windowPreferences->Save(preferences) ? "" : "Settings could not save";
		}
	}
	ImGui::EndDisabled();
	if (services.windowPreferences == nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("Choose game folder first");
	}
	TransferSettings::Draw(state, services);
	TrustSettings::Draw(services.registry);
	UpdateSettings::Draw(state, services.updates);
	ImGui::Separator();
	if (ImGui::Button("Done") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		state.CloseSettings();
		ImGui::CloseCurrentPopup();
	}
}
}
