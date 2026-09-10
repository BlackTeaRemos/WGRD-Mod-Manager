#include "gui/shell/StatusBar.h"

#include "domain/BuildInfo.h"
#include "domain/types/distribution/RepositoryUri.h"
#include <algorithm>
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"

#include <format>
#include <string>

namespace wgrd::gui {
float StatusBar::DrawManager_(const ImVec2 origin, float cursor, const bool detailed,
	ApplicationState& state, const ApplicationServices& services) {
	if (detailed) {
		const std::string repository(domain::RepositoryUri::Slug(domain::build::RELEASE_REPOSITORY));
		const float repositoryWidth = std::min(180.0f, design::MeasureText(repository, 10.0f).x);
		design::ClippedText::Draw(ImVec2(cursor, origin.y + 5.0f), repositoryWidth, repository, tokens::ACCENT, 10.0f);
		cursor += repositoryWidth + 6.0f;
		const std::string version = std::string("v") + std::string(domain::build::VERSION);
		design::TextAt(ImVec2(cursor, origin.y + 5.0f), version, tokens::SECONDARY, 10.0f);
		cursor += design::MeasureText(version, 10.0f).x + 8.0f;
	}
	const auto status = services.updates != nullptr ? services.updates->Status() : domain::UpdateStatus{};
	const std::string_view action = status.phase == domain::UpdatePhase::Ready ? "Restart"
		: (status.phase == domain::UpdatePhase::Available ? "Update" : "Check updates");
	if (design::Button(ImVec2(cursor, origin.y + 1.0f), action, design::ButtonVariant::Neutral,
		false, 6.0f, services.updates != nullptr && !status.Busy())) {
		if (status.phase == domain::UpdatePhase::Ready) {
			if (services.updates->ApplyAndRestart()) {
				state.RequestExit();
			}
		} else if (status.phase == domain::UpdatePhase::Available) {
			services.updates->Download();
		} else {
			services.updates->Check();
		}
	}
	return cursor + design::ButtonSize(action, 6.0f).x + 8.0f;
}

float StatusBar::DrawPatcher_(const ImVec2 origin, float cursor, const bool detailed,
	const ApplicationServices& services) {
	if (services.patcher == nullptr) {
		return cursor;
	}
	const auto status = services.patcher->Status();
	if (detailed) {
		const std::string repository(domain::RepositoryUri::Slug(domain::build::PATCHER_REPOSITORY));
		const float repositoryWidth = std::min(180.0f, design::MeasureText(repository, 10.0f).x);
		design::ClippedText::Draw(ImVec2(cursor, origin.y + 5.0f), repositoryWidth, repository, tokens::ACCENT, 10.0f);
		cursor += repositoryWidth + 6.0f;
		const std::string version = status.present ? (status.installedTag.empty() ? "installed" : status.installedTag) : "missing";
		const float versionWidth = std::min(120.0f, design::MeasureText(version, 10.0f).x);
		design::ClippedText::Draw(ImVec2(cursor, origin.y + 5.0f), versionWidth, version, status.present ? tokens::SUCCESS : tokens::ADVISORY, 10.0f);
		cursor += versionWidth + 8.0f;
	}
	const bool offered = status.phase == domain::UpdatePhase::Available;
	const std::string_view action = offered ? (status.present ? "Update patcher" : "Install patcher") : "Check patcher";
	if (design::Button(ImVec2(cursor, origin.y + 1.0f), action, design::ButtonVariant::Neutral, false, 6.0f, !status.Busy())) {
		if (offered) {
			services.patcher->Install();
		} else {
			services.patcher->Check();
		}
	}
	return cursor + design::ButtonSize(action, 6.0f).x + 8.0f;
}

void StatusBar::Draw(const ImVec2 origin, const float width, ApplicationState& state, const ApplicationServices& services) {
	design::FillRect(origin, ImVec2(origin.x + width, origin.y + tokens::STATUS_BAR_HEIGHT), tokens::SURFACE_RAISED);
	design::HorizontalRule(origin.x, origin.x + width, origin.y, tokens::BORDER);
	const bool detailed = width >= 1000.0f;
	float cursor = DrawManager_(origin, origin.x + 6.0f, detailed, state, services);
	cursor = DrawPatcher_(origin, cursor, detailed, services);
	if (design::Button(ImVec2(cursor, origin.y + 1.0f), "Support", design::ButtonVariant::Accent, false, 6.0f)) {
		ImGui::OpenPopup("Support");
	}
	cursor += design::ButtonSize("Support", 6.0f).x + 8.0f;
	_support.Draw(services);

	std::string message;
	ImU32 tone = tokens::SECONDARY_MUTED;
	if (services.install != nullptr && services.install->Progress().Busy()) {
		message = services.install->Progress().message;
	} else if (services.baseline != nullptr && services.baseline->Status().running) {
		const auto baseline = services.baseline->Status();
		message = std::format("Indexing {} of {}", baseline.indexedFiles + baseline.failedFiles, baseline.totalFiles);
		tone = tokens::ACCENT;
	} else if (services.patcher != nullptr && !services.patcher->Status().message.empty()) {
		const auto status = services.patcher->Status();
		message = status.message;
		tone = status.phase == domain::UpdatePhase::Failed ? tokens::FAILURE : tone;
	} else if (services.updates != nullptr) {
		const auto status = services.updates->Status();
		message = status.message;
		tone = status.phase == domain::UpdatePhase::Failed ? tokens::FAILURE : tone;
	}
	if (services.order == nullptr) {
		message = "Game folder missing";
		tone = tokens::FAILURE;
	}
	design::ClippedText::Draw(ImVec2(cursor, origin.y + 5.0f),
		origin.x + width - cursor - 6.0f, message, tone, 10.0f);
}
}
