#include "gui/modals/settings/TrustSettings.h"
#include "gui/modals/ModalSection.h"
#include <format>

#include "gui/design/Tokens.h"

#include <imgui.h>

namespace wgrd::gui {
void TrustSettings::Draw(domain::IRegistryUpdater* registry) {
	ModalSection::Draw("Trust registry");
	if (registry == nullptr) {
		ImGui::TextDisabled("Registry unavailable");
		return;
	}
	const auto status = registry->Status();
	ImGui::TextUnformatted(std::format("{} keys trusted", status.keyCount).c_str());
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(status.phase == domain::RegistryPhase::Failed ? tokens::FAILURE : tokens::SECONDARY),
		"%s", status.message.c_str());
	ImGui::BeginDisabled(status.Busy());
	if (ImGui::Button(status.phase == domain::RegistryPhase::Failed ? "Retry registry" : "Sync registry")) {
		registry->Poll();
	}
	ImGui::EndDisabled();
	if (status.phase == domain::RegistryPhase::Failed) {
		ImGui::SameLine();
		if (ImGui::Button("Copy error")) {
			ImGui::SetClipboardText(status.message.c_str());
		}
	}
}
}
