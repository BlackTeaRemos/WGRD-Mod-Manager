#include "gui/modals/settings/TransferSettings.h"
#include "gui/modals/ModalSection.h"
#include "gui/design/Tokens.h"

#include <imgui.h>
#include <array>
#include <format>

namespace wgrd::gui {
void TransferSettings::Draw(ApplicationState& state, const ApplicationServices& services) {
	(void)state;
	ModalSection::Draw("Transfers");
	if (services.seeding != nullptr) {
		bool enabled = services.seeding->Enabled();
		if (ImGui::Checkbox("Seed installed mods", &enabled)) {
			services.seeding->SetEnabled(enabled);
		}
	}
	if (services.mirror != nullptr) {
		const auto status = services.mirror->Status();
		bool enabled = status.enabled;
		if (ImGui::Checkbox("Mirror every mod", &enabled)) {
			services.mirror->SetEnabled(enabled);
		}
		if (enabled) {
			ImGui::TextUnformatted(std::format("{} waiting", status.pending).c_str());
		}
	}
	if (services.swarm == nullptr) {
		return;
	}
	ImGui::TextUnformatted("Link budget");
	constexpr std::array<std::int64_t, 6> PRESETS = {0, 2, 8, 25, 100, 500};
	const std::int64_t current = services.swarm->LinkBudget();
	for (const std::int64_t preset : PRESETS) {
		const std::int64_t bytes = preset * 1024LL * 1024LL;
		const std::string label = preset == 0 ? "Unlimited" : std::format("{} MiB/s", preset);
		ImGui::PushStyleColor(ImGuiCol_Button, current == bytes ? tokens::ACCENT_ACTIVE_FILL : tokens::SURFACE_RAISED);
		if (ImGui::Button(label.c_str())) {
			services.swarm->SetLinkBudget(bytes);
		}
		ImGui::PopStyleColor();
		if (preset != PRESETS.back() && ImGui::GetContentRegionAvail().x > 90.0f) {
			ImGui::SameLine();
		}
	}
	ImGui::TextDisabled("Reserves control bandwidth");
}
}
