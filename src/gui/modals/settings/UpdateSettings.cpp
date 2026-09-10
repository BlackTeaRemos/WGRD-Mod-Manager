#include "gui/modals/settings/UpdateSettings.h"
#include "gui/modals/ModalSection.h"

#include "gui/design/Tokens.h"

#include <imgui.h>
#include <algorithm>

namespace wgrd::gui {
void UpdateSettings::Draw(ApplicationState& state, domain::IUpdateService* updates) {
	ModalSection::Draw("Application updates");
	if (updates == nullptr) {
		ImGui::TextDisabled("Updates unavailable");
		return;
	}
	const auto status = updates->Status();
	ImGui::Text("Installed %s", status.currentVersion.c_str());
	if (!status.latestVersion.empty()) {
		ImGui::Text("Latest %s", status.latestVersion.c_str());
	}
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(status.phase == domain::UpdatePhase::Failed ? tokens::FAILURE : tokens::SECONDARY),
		"%s", status.message.c_str());
	if (status.Busy()) {
		if (status.totalBytes != 0) {
			const float fraction = static_cast<float>(static_cast<double>(status.downloadedBytes) / static_cast<double>(status.totalBytes));
			ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-1.0f, 9.0f), "");
		}
		return;
	}
	if (status.phase == domain::UpdatePhase::Available) {
		if (ImGui::Button("Download update")) {
			updates->Download();
		}
	} else if (status.phase == domain::UpdatePhase::Ready) {
		if (ImGui::Button("Restart and update") && updates->ApplyAndRestart()) {
			state.RequestExit();
		}
	} else if (ImGui::Button(status.phase == domain::UpdatePhase::Failed ? "Retry update check" : "Check for updates")) {
		updates->Check();
	}
	if (status.phase == domain::UpdatePhase::Failed) {
		ImGui::SameLine();
		if (ImGui::Button("Copy error")) {
			ImGui::SetClipboardText(status.message.c_str());
		}
	}
}
}
