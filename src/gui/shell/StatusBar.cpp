#include "gui/shell/StatusBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/text/CommonText.h"
#include "gui/text/ShellText.h"

#include <format>
#include <string>

namespace wgrd::gui {
namespace {
	constexpr float BUTTON_PADDING = 6.0f;
	constexpr float SEGMENT_GAP = 8.0f;

	std::string RepositoryLabel(const std::string_view repository) {
		if (repository.empty()) {
			return std::string(text::shell::LOCAL_BUILD);
		}

		return std::string(repository);
	}
}

float StatusBar::DrawSeparator_(const ImVec2 origin, const ImVec2 bottomRight, const float cursor) {
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(cursor, origin.y),
		ImVec2(cursor, bottomRight.y),
		tokens::BORDER
	);

	return cursor + SEGMENT_GAP;
}

float StatusBar::DrawManager_(
	const ImVec2 origin,
	const ImVec2 bottomRight,
	float cursor,
	const ApplicationServices& services
) {
	const float baseline = origin.y + 5.0f;

	design::TextAt(ImVec2(cursor, baseline), text::shell::MANAGER_LABEL, tokens::SECONDARY_MUTED, 10.0f);
	cursor += design::MeasureText(text::shell::MANAGER_LABEL, 10.0f).x + 6.0f;

	const std::string repository = RepositoryLabel(domain::build::RELEASE_REPOSITORY);
	design::TextAt(ImVec2(cursor, baseline), repository, tokens::ACCENT, 10.0f);
	cursor += design::MeasureText(repository, 10.0f).x + 6.0f;

	const std::string version = std::string("v") + std::string(domain::build::VERSION);
	design::TextAt(ImVec2(cursor, baseline), version, tokens::SECONDARY, 10.0f);
	cursor += design::MeasureText(version, 10.0f).x + 6.0f;

	if (services.updates == nullptr) {
		return DrawSeparator_(origin, bottomRight, cursor);
	}

	const domain::UpdateStatus status = services.updates->Status();

	const bool ready = status.phase == domain::UpdatePhase::Ready;
	const bool available = status.phase == domain::UpdatePhase::Available;

	std::string_view action = text::shell::CHECK;

	if (ready) {
		action = text::shell::RESTART;
	} else if (available) {
		action = text::shell::UPDATE;
	}

	if (design::Button(
		ImVec2(cursor, origin.y + 1.0f),
		action,
		ready || available ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral,
		ready,
		BUTTON_PADDING,
		!status.Busy()
	)) {
		if (ready) {
			const bool restarted = services.updates->ApplyAndRestart();
			(void)restarted;
		} else if (available) {
			services.updates->Download();
		} else {
			services.updates->Check();
		}
	}

	cursor += design::ButtonSize(action, BUTTON_PADDING).x + SEGMENT_GAP;

	return DrawSeparator_(origin, bottomRight, cursor);
}

float StatusBar::DrawPatcher_(
	const ImVec2 origin,
	float cursor,
	const ApplicationServices& services
) {
	const float baseline = origin.y + 5.0f;

	design::TextAt(ImVec2(cursor, baseline), text::shell::PATCHER_LABEL, tokens::SECONDARY_MUTED, 10.0f);
	cursor += design::MeasureText(text::shell::PATCHER_LABEL, 10.0f).x + 6.0f;

	const std::string repository = RepositoryLabel(domain::build::PATCHER_REPOSITORY);
	design::TextAt(ImVec2(cursor, baseline), repository, tokens::ACCENT, 10.0f);
	cursor += design::MeasureText(repository, 10.0f).x + 6.0f;

	if (services.patcher == nullptr) {
		return cursor;
	}

	const domain::PatcherStatus status = services.patcher->Status();

	std::string version(text::shell::NOT_INSTALLED);
	ImU32 versionTone = tokens::FAILURE;

	if (status.present) {
		if (!status.installedTag.empty()) {
			version = status.installedTag;
			versionTone = tokens::SUCCESS;
		} else if (!status.runtimeStamp.empty()) {
			version = status.runtimeStamp;
			versionTone = tokens::SECONDARY;
		} else {
			version = std::string(text::shell::UNKNOWN_VERSION);
			versionTone = tokens::ADVISORY;
		}
	}

	design::TextAt(ImVec2(cursor, baseline), version, versionTone, 10.0f);
	cursor += design::MeasureText(version, 10.0f).x + 6.0f;

	const bool offered = status.phase == domain::UpdatePhase::Available;

	std::string_view action = text::shell::CHECK;

	if (offered) {
		action = status.present ? text::shell::UPDATE : text::shell::INSTALL;
	}

	if (design::Button(
		ImVec2(cursor, origin.y + 1.0f),
		action,
		offered ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral,
		false,
		BUTTON_PADDING,
		!status.Busy()
	)) {
		if (offered) {
			services.patcher->Install();
		} else {
			services.patcher->Check();
		}
	}

	cursor += design::ButtonSize(action, BUTTON_PADDING).x + SEGMENT_GAP;

	return cursor;
}

void StatusBar::Draw(const ImVec2 origin, const float width, const ApplicationServices& services) {
	const ImVec2 bottomRight(origin.x + width, origin.y + tokens::STATUS_BAR_HEIGHT);

	design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);
	design::HorizontalRule(origin.x, bottomRight.x, origin.y, tokens::BORDER);

	float cursor = origin.x + 8.0f;

	cursor = DrawManager_(origin, bottomRight, cursor, services);
	cursor = DrawPatcher_(origin, cursor, services);

	std::string message;

	if (services.patcher != nullptr && !services.patcher->Status().message.empty()) {
		message = services.patcher->Status().message;
	} else if (services.updates != nullptr) {
		message = services.updates->Status().message;
	}

	if (services.order == nullptr) {
		message = std::string(text::common::GAME_MISSING);
	}

	if (message.empty()) {
		return;
	}

	const float messageWidth = design::MeasureText(message, 10.0f).x;

	design::TextAt(
		ImVec2(bottomRight.x - messageWidth - 8.0f, origin.y + 5.0f),
		message,
		services.order == nullptr ? tokens::FAILURE : tokens::SECONDARY_MUTED,
		10.0f
	);
}
}
