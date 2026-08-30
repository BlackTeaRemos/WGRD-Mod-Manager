#include "gui/shell/StatusBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/text/CommonText.h"
#include "gui/text/ShellText.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace wgrd::gui {
namespace {
	constexpr float BUTTON_PADDING = 6.0f;
	constexpr float SEGMENT_GAP = 8.0f;

	constexpr double SUPPORT_CYCLE_SECONDS = 10.0;
	constexpr float SUPPORT_PANEL_WIDTH = 460.0f;
	constexpr float SUPPORT_PANEL_HEIGHT = 190.0f;
	constexpr float SUPPORT_ROW_STEP = 16.0f;

	constexpr float SUPPORT_HOVER_FILL = 0.14f;

	ImU32 FadeTone(const ImU32 tone, const float alpha) {
		ImVec4 channels = ImGui::ColorConvertU32ToFloat4(tone);
		channels.w = alpha;

		return ImGui::ColorConvertFloat4ToU32(channels);
	}

	ImU32 BlendTone(const ImU32 from, const ImU32 to, const float amount) {
		const ImVec4 start = ImGui::ColorConvertU32ToFloat4(from);
		const ImVec4 finish = ImGui::ColorConvertU32ToFloat4(to);

		return ImGui::ColorConvertFloat4ToU32(ImVec4(
				start.x + (finish.x - start.x) * amount,
				start.y + (finish.y - start.y) * amount,
				start.z + (finish.z - start.z) * amount,
				start.w + (finish.w - start.w) * amount
			)
		);
	}

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

ImU32 StatusBar::SupportTone_() {
	const double elapsed = ImGui::GetTime();
	const double phase = elapsed / SUPPORT_CYCLE_SECONDS - std::floor(elapsed / SUPPORT_CYCLE_SECONDS);

	if (phase < 0.5) {
		return BlendTone(tokens::SUPPORT_DARK, tokens::SUPPORT_AMBER, static_cast<float>(phase * 2.0));
	}

	return BlendTone(tokens::SUPPORT_AMBER, tokens::SUPPORT_ORANGE, static_cast<float>((phase - 0.5) * 2.0));
}

float StatusBar::DrawSupport_(const ImVec2 origin, float cursor) {
	const ImVec2 size = design::ButtonSize(text::shell::SUPPORT, BUTTON_PADDING);
	const ImVec2 topLeft(cursor, origin.y + 1.0f);
	const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

	const ImU32 tone = SupportTone_();

	bool hovered = false;
	const bool clicked = design::RowHit(topLeft, bottomRight, hovered);

	if (hovered || _supportOpen) {
		design::FillRect(topLeft, bottomRight, FadeTone(tone, SUPPORT_HOVER_FILL));
	}

	design::StrokeRect(topLeft, bottomRight, tone);

	design::TextAt(
		ImVec2(topLeft.x + BUTTON_PADDING, topLeft.y + 4.0f),
		text::shell::SUPPORT,
		tone,
		10.0f
	);

	if (clicked) {
		_supportOpen = !_supportOpen;
	}

	return cursor + size.x + SEGMENT_GAP;
}

void StatusBar::DrawSupportPanel_(
	const ImVec2 origin,
	const float anchorX,
	const ApplicationServices& services
) const {
	if (!_supportOpen) {
		return;
	}

	const ImVec2 topLeft(anchorX, origin.y - SUPPORT_PANEL_HEIGHT);
	const ImVec2 bottomRight(topLeft.x + SUPPORT_PANEL_WIDTH, origin.y);

	design::Shadow(topLeft, bottomRight);
	design::FillRect(topLeft, bottomRight, tokens::SURFACE_RAISED);
	design::StrokeRect(topLeft, bottomRight, tokens::BORDER);

	float rowY = topLeft.y + 8.0f;

	design::TextAt(
		ImVec2(topLeft.x + 10.0f, rowY),
		text::shell::SUPPORT_PANEL,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	rowY += SUPPORT_ROW_STEP;

	design::TextAt(ImVec2(topLeft.x + 10.0f, rowY), text::shell::GETLY, tokens::SECONDARY, 11.0f);

	const ImVec2 getlyExtent = design::MeasureText(text::shell::GETLY_URI, 10.0f);
	const ImVec2 getlyTopLeft(topLeft.x + 70.0f, rowY);
	const ImVec2 getlyBottomRight(getlyTopLeft.x + getlyExtent.x, getlyTopLeft.y + 12.0f);

	bool getlyHovered = false;
	const bool getlyClicked = design::RowHit(getlyTopLeft, getlyBottomRight, getlyHovered);

	design::TextAt(
		getlyTopLeft,
		text::shell::GETLY_URI,
		getlyHovered ? tokens::ACCENT_HOVER : tokens::ACCENT,
		10.0f
	);

	if (getlyClicked && services.uris != nullptr) {
		const bool opened = services.uris->Open(text::shell::GETLY_URI);
		(void)opened;
	}

	rowY += SUPPORT_ROW_STEP + 6.0f;

	design::TextAt(ImVec2(topLeft.x + 10.0f, rowY), text::shell::PATREON, tokens::SECONDARY, 11.0f);

	rowY += SUPPORT_ROW_STEP;

	design::TextAt(
		ImVec2(topLeft.x + 10.0f, rowY),
		text::shell::PATREON_HINT_LEAD,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	rowY += 13.0f;

	const ImVec2 discordExtent = design::MeasureText(text::shell::DISCORD_URI, 10.0f);
	const ImVec2 discordTopLeft(topLeft.x + 10.0f, rowY);
	const ImVec2 discordBottomRight(discordTopLeft.x + discordExtent.x, discordTopLeft.y + 12.0f);

	bool discordHovered = false;
	const bool discordClicked = design::RowHit(discordTopLeft, discordBottomRight, discordHovered);

	design::TextAt(
		discordTopLeft,
		text::shell::DISCORD_URI,
		discordHovered ? tokens::ACCENT_HOVER : tokens::ACCENT,
		10.0f
	);

	if (discordClicked && services.uris != nullptr) {
		const bool opened = services.uris->Open(text::shell::DISCORD_URI);
		(void)opened;
	}

	design::TextAt(
		ImVec2(discordTopLeft.x + discordExtent.x + 6.0f, rowY),
		text::shell::PATREON_HINT_TAIL,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	rowY += SUPPORT_ROW_STEP + 8.0f;

	design::TextAt(
		ImVec2(topLeft.x + 10.0f, rowY),
		text::shell::STAR_REPOSITORIES,
		tokens::SECONDARY,
		10.0f
	);

	rowY += 15.0f;

	const std::array<std::string_view, 4> repositories = {
		domain::build::SOURCE_REPOSITORY,
		domain::build::RELEASE_REPOSITORY,
		domain::build::INDEX_REPOSITORY,
		domain::build::PATCHER_REPOSITORY
	};

	std::vector<std::string_view> listed;

	for (const std::string_view repository : repositories) {
		if (repository.empty()) {
			continue;
		}

		if (std::ranges::find(listed, repository) != listed.end()) {
			continue;
		}

		listed.push_back(repository);

		const std::string uri = std::string(text::shell::URI_PREFIX) + std::string(repository);
		const ImVec2 extent = design::MeasureText(uri, 10.0f);
		const ImVec2 linkTopLeft(topLeft.x + 10.0f, rowY);
		const ImVec2 linkBottomRight(linkTopLeft.x + extent.x, linkTopLeft.y + 12.0f);

		bool linkHovered = false;
		const bool linkClicked = design::RowHit(linkTopLeft, linkBottomRight, linkHovered);

		design::TextAt(
			linkTopLeft,
			uri,
			linkHovered ? tokens::ACCENT_HOVER : tokens::ACCENT,
			10.0f
		);

		if (linkClicked && services.uris != nullptr) {
			const bool opened = services.uris->Open(uri);
			(void)opened;
		}

		rowY += 13.0f;
	}
}

void StatusBar::Draw(const ImVec2 origin, const float width, const ApplicationServices& services) {
	const ImVec2 bottomRight(origin.x + width, origin.y + tokens::STATUS_BAR_HEIGHT);

	design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);
	design::HorizontalRule(origin.x, bottomRight.x, origin.y, tokens::BORDER);

	float cursor = origin.x + 8.0f;

	cursor = DrawManager_(origin, bottomRight, cursor, services);
	cursor = DrawPatcher_(origin, cursor, services);

	const float supportAnchor = cursor;
	cursor = DrawSupport_(origin, cursor);

	DrawSupportPanel_(origin, supportAnchor, services);

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
	const float messageX = std::max(cursor, bottomRight.x - messageWidth - 8.0f);

	design::TextAt(
		ImVec2(messageX, origin.y + 5.0f),
		message,
		services.order == nullptr ? tokens::FAILURE : tokens::SECONDARY_MUTED,
		10.0f
	);
}
}
