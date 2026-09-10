#include "gui/shell/support/SupportPanel.h"

#include "domain/BuildInfo.h"
#include "domain/types/distribution/RepositoryUri.h"
#include "gui/design/FontLibrary.h"
#include "gui/design/Tokens.h"
#include "gui/modals/ModalSection.h"
#include "gui/modals/ModalSurface.h"
#include "gui/text/ShellText.h"

#include <algorithm>
#include <array>
#include <vector>

namespace wgrd::gui {
void SupportPanel::Link_(const std::string_view label, const std::string_view uri, const ApplicationServices& services) {
	ImGui::PushID(uri.data(), uri.data() + uri.size());
	ImGui::BeginDisabled(services.uris == nullptr);
	if (ImGui::Button(std::string(label).c_str())) {
		_notice = services.uris->Open(uri) ? "" : "Link could not open";
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", std::string(uri).c_str());
	}
	ImGui::PopID();
}

void SupportPanel::Draw(const ApplicationServices& services) {
	if (!ModalSurface::BeginMenu("Support", 320.0f)) {
		return;
	}
	ModalSection::Draw("Support");
	Link_(text::shell::GETLY, text::shell::GETLY_URI, services);
	ImGui::TextUnformatted("Patreon through Discord");
	Link_("Contact through Discord", text::shell::DISCORD_URI, services);
	ModalSection::Draw("Repositories");
	const std::array<std::string_view, 5> repositories = {
		domain::build::SOURCE_REPOSITORY, domain::build::RELEASE_REPOSITORY,
		domain::build::INDEX_REPOSITORY, domain::build::PATCHER_REPOSITORY, domain::build::TOOLKIT_REPOSITORY
	};
	std::vector<std::string_view> listed;
	for (const std::string_view repository : repositories) {
		if (repository.empty() || std::ranges::find(listed, repository) != listed.end()) {
			continue;
		}
		listed.push_back(repository);
		Link_(domain::RepositoryUri::Slug(repository), domain::RepositoryUri::Https(repository), services);
	}
	if (!_notice.empty()) {
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(tokens::FAILURE), "%s", _notice.c_str());
		if (ImGui::Button("Copy error")) {
			ImGui::SetClipboardText(_notice.c_str());
		}
	}
	ModalSurface::End();
}
}
