#include "gui/modals/details/ModDetailsPanel.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/Tokens.h"
#include "gui/feedback/InstallStartText.h"
#include "gui/modals/ModalSection.h"
#include "gui/screens/ScreenToolbar.h"

#include <algorithm>
#include <format>

namespace wgrd::gui {
void ModDetailsPanel::Start_(const bool verify, const std::string& identifier,
	ApplicationState& state, const ApplicationServices& services) {
	_verify = verify;
	_retryInstall = true;
	const auto result = verify ? services.install->Verify(identifier) : services.install->Start(identifier);
	if (!result.has_value()) {
		_notice = InstallStartText::Describe(result.error());
		return;
	}
	_notice.clear();
	state.CloseDetail();
	state.SetScreen(Screen::Transfers);
	ImGui::CloseCurrentPopup();
}

void ModDetailsPanel::Draw(ApplicationState& state, const ApplicationServices& services) {
	if (_shownIdentifier != state.DetailTarget()) {
		_shownIdentifier = state.DetailTarget();
		_notice.clear();
		_retryInstall = false;
	}
	const domain::CatalogRow* row = nullptr;
	if (services.catalog != nullptr) {
		const auto& rows = services.catalog->Rows();
		const auto found = std::ranges::find_if(rows, [&state](const domain::CatalogRow& candidate) {
			return candidate.identifier == state.DetailTarget();
		});
		if (found != rows.end()) {
			row = &*found;
		}
	}
	if (row == nullptr) {
		ImGui::TextUnformatted("Mod no longer available");
	} else {
		ImGui::PushFont(design::FontLibrary::For(FontRole::Name), 12.0f);
		ImGui::TextUnformatted(row->modName.c_str());
		ImGui::PopFont();
		ImGui::TextUnformatted(std::format("Version {}", row->version).c_str());
		if (row->manifestHeld) {
			ImGui::TextUnformatted(ScreenToolbar::FormatBytes(row->totalBytes).c_str());
			ImGui::TextUnformatted(std::format("{} files", row->fileCount).c_str());
			ImGui::TextUnformatted(std::format("{} chunks", row->chunkCount).c_str());
		}
		ModalSection::Draw("Identity");
		ImGui::TextWrapped("%s", row->identifier.c_str());
		if (ImGui::Button("Copy identifier")) {
			ImGui::SetClipboardText(row->identifier.c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy fingerprint")) {
			ImGui::SetClipboardText(row->identifier.substr(0, row->identifier.find('/')).c_str());
		}
		if (row->installed && services.order != nullptr && services.paths != nullptr) {
			const auto& snapshot = services.order->Current();
			const std::filesystem::path folder = std::filesystem::path(snapshot.gameRoot) / snapshot.modsDirectory / row->modName;
			if (ImGui::Button("Copy folder path")) {
				ImGui::SetClipboardText(folder.string().c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("Open mod folder")) {
				_retryInstall = false;
				_notice = services.paths->OpenFolder(folder).has_value() ? "" : "Folder could not open";
			}
		}
		ModalSection::Draw("Verification");
		ImGui::TextUnformatted(row->revoked ? "Publisher key revoked" : (row->manifestHeld ? "Manifest verified" : "Manifest retrieved on install"));
		if (services.install != nullptr) {
			const bool enabled = !services.install->Progress().Busy() && !row->revoked;
			ImGui::BeginDisabled(!enabled);
			if (ImGui::Button(row->installed ? "Reinstall" : "Install")) {
				Start_(false, row->identifier, state, services);
			}
			ImGui::EndDisabled();
			if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("%s", row->revoked ? "Publisher key revoked" : "Another operation active");
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!enabled || !row->installed || !row->manifestHeld);
			if (ImGui::Button("Verify files")) {
				Start_(true, row->identifier, state, services);
			}
			ImGui::EndDisabled();
			if ((!enabled || !row->installed || !row->manifestHeld) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("%s", !enabled ? "Install action unavailable" : (!row->installed ? "Install before verifying" : "Manifest not available"));
			}
			if (!_notice.empty()) {
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(tokens::FAILURE), "%s", _notice.c_str());
				if (ImGui::Button("Copy error")) {
					ImGui::SetClipboardText(_notice.c_str());
				}
				if (_retryInstall) {
					ImGui::SameLine();
					ImGui::BeginDisabled(!enabled);
					if (ImGui::Button("Retry")) {
						Start_(_verify, row->identifier, state, services);
					}
					ImGui::EndDisabled();
				}
			}
		}
	}
	ImGui::Separator();
	if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		state.CloseDetail();
		ImGui::CloseCurrentPopup();
	}
}
}
