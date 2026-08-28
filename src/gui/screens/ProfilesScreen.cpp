#include "gui/screens/ProfilesScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/ProfilesText.h"

#include <algorithm>
#include <format>
#include <vector>

namespace wgrd::gui {
namespace {
	constexpr float DETAIL_ROW_HEIGHT = 20.0f;
}

ImVec2 ProfilesScreen::HeadingOrigin_(const ScreenArea& area, const float cursorY) {
	return ImVec2(area.origin.x + LIST_WIDTH + 1.0f, cursorY);
}

float ProfilesScreen::HeadingWidth_(const ScreenArea& area) {
	return area.width - LIST_WIDTH - 1.0f;
}

const domain::ProfileSummary* ProfilesScreen::Selected_(
	const ApplicationState& state,
	domain::IProfileService* profiles
) const {
	const std::vector<domain::ProfileSummary>& rows = profiles->Profiles();

	if (rows.empty()) {
		return nullptr;
	}

	const std::size_t index = state.SelectedProfile() < rows.size() ? state.SelectedProfile() : 0;

	return &rows[index];
}

float ProfilesScreen::DrawList_(
	const ScreenArea& area,
	const float cursorY,
	ApplicationState& state,
	domain::IProfileService* profiles
) {
	const std::vector<domain::ProfileSummary>& rows = profiles->Profiles();

	if (rows.empty()) {
		ScreenToolbar::Placeholder(area, cursorY, text::profiles::EMPTY);
		return cursorY + 34.0f;
	}

	float rowY = cursorY;

	for (std::size_t index = 0; index < rows.size(); ++index) {
		const domain::ProfileSummary& row = rows[index];

		const ImVec2 rowTopLeft(area.origin.x, rowY);
		const ImVec2 rowBottomRight(area.origin.x + LIST_WIDTH, rowY + ROW_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
		const bool selected = index == state.SelectedProfile();

		if (selected) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
		} else if (hovered) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
		}

		design::TextAt(
			ImVec2(rowTopLeft.x + 8.0f, rowY + 5.0f),
			row.name,
			selected ? tokens::ACCENT : tokens::SECONDARY,
			12.0f
		);

		design::TextAt(
			ImVec2(rowTopLeft.x + 8.0f, rowY + 20.0f),
			std::format(text::profiles::ENABLED_OF_FORMAT, row.enabledCount, row.entryCount),
			tokens::SECONDARY_MUTED,
			9.0f
		);

		if (row.active) {
			design::Pill(ImVec2(rowBottomRight.x - 64.0f, rowY + 6.0f), text::profiles::BADGE_ACTIVE, tokens::SUCCESS);
		} else if (row.missingCount > 0) {
			design::Pill(ImVec2(rowBottomRight.x - 64.0f, rowY + 6.0f), text::profiles::BADGE_MISSING, tokens::FAILURE);
		}

		design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

		if (clicked) {
			state.SelectProfile(index);
		}

		rowY += ROW_HEIGHT;
	}

	return rowY;
}

float ProfilesScreen::DrawGameProfiles_(
	const ScreenArea& area,
	float cursorY,
	domain::IProfileService* profiles
) {
	const float left = area.origin.x + LIST_WIDTH + 12.0f;
	const float width = area.width - LIST_WIDTH - 18.0f;

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::GAME_PROFILES,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const std::vector<domain::GameProfileFile>& discovered = profiles->Discovered();

	if (discovered.empty()) {
		design::TextAt(
			ImVec2(left, cursorY + 4.0f),
			text::profiles::NO_GAME_PROFILES,
			tokens::TEXT_DISABLED,
			10.0f
		);

		return cursorY + 22.0f;
	}

	std::string account;

	for (const domain::GameProfileFile& candidate : discovered) {
		if (candidate.account != account) {
			account = candidate.account;

			design::TextAt(
				ImVec2(left, cursorY + 4.0f),
				std::format(text::profiles::ACCOUNT_FORMAT, account),
				tokens::SECONDARY_MUTED,
				9.0f
			);

			cursorY += 16.0f;
		}

		design::TextAt(
			ImVec2(left + 10.0f, cursorY + 4.0f),
			candidate.name,
			candidate.live ? tokens::ACCENT : tokens::SECONDARY,
			11.0f
		);

		if (candidate.live) {
			design::TextAt(
				ImVec2(left + 170.0f, cursorY + 5.0f),
				text::profiles::LIVE_MARK,
				tokens::SUCCESS,
				9.0f
			);
		}

		design::TextAt(
			ImVec2(left + 210.0f, cursorY + 5.0f),
			ScreenToolbar::FormatBytes(candidate.sizeBytes),
			tokens::SECONDARY_MUTED,
			9.0f
		);

		if (design::Button(
			ImVec2(left + width - 84.0f, cursorY + 2.0f),
			text::profiles::SET_DEFAULT,
			design::ButtonVariant::Neutral,
			false,
			8.0f
		)) {
			_notice.clear();

			const auto assigned = profiles->SetDefault(candidate);
			(void)assigned;
		}

		cursorY += DETAIL_ROW_HEIGHT;
		design::HorizontalRule(left, left + width, cursorY, tokens::BORDER_SUBTLE);
	}

	return cursorY + 8.0f;
}

void ProfilesScreen::DrawDetail_(
	const ScreenArea& area,
	float cursorY,
	ApplicationState& state,
	domain::IProfileService* profiles,
	domain::IOrderService* order
) {
	const float left = area.origin.x + LIST_WIDTH + 12.0f;
	const float width = area.width - LIST_WIDTH - 18.0f;

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(area.origin.x + LIST_WIDTH, cursorY),
		ImVec2(area.origin.x + LIST_WIDTH, area.origin.y + area.height),
		tokens::BORDER
	);

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::CAPTURE_LABEL,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor) + 4.0f;

	const ImVec2 captureSize = design::ButtonSize(
		text::profiles::CAPTURE,
		11.0f,
		design::FIELD_HEIGHT
	);

	design::TextField(
		ImVec2(left, cursorY),
		width - captureSize.x - 6.0f,
		text::profiles::NAME_FIELD,
		_nameBuffer.data(),
		_nameBuffer.size()
	);

	if (design::Button(
		ImVec2(left + width - captureSize.x, cursorY),
		text::profiles::CAPTURE,
		design::ButtonVariant::Accent,
		true,
		11.0f,
		true,
		design::FIELD_HEIGHT
	)) {
		_notice.clear();

		const auto captured = profiles->CaptureCurrent(_nameBuffer.data());
		if (captured.has_value()) {
			_nameBuffer.fill('\0');
		}
	}

	cursorY += design::FIELD_HEIGHT + 8.0f;

	const domain::ProfileSummary* const selected = Selected_(state, profiles);

	if (selected == nullptr) {
		design::TextAt(
			ImVec2(left, cursorY),
			profiles->LastMessage(),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		cursorY = DrawGameProfiles_(area, cursorY + 18.0f, profiles);
		return;
	}

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		selected->name,
		design::HeadingLevel::Minor,
		selected->active ? design::HeadingTone::Success : design::HeadingTone::Accent
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor) + 4.0f;

	if (design::Button(
		ImVec2(left, cursorY),
		selected->active ? text::profiles::FORCE_SELECT : text::profiles::SELECT,
		design::ButtonVariant::Accent,
		true,
		11.0f
	)) {
		_notice.clear();

		const auto activated = profiles->Activate(selected->name);
		if (activated.has_value() && order != nullptr) {
			order->Refresh();
		}
	}

	if (design::Button(
		ImVec2(left + 86.0f, cursorY),
		text::profiles::CLONE,
		design::ButtonVariant::Neutral,
		false,
		11.0f
	)) {
		_notice.clear();

		const auto cloned = profiles->Clone(selected->name, _nameBuffer.data());
		if (cloned.has_value()) {
			_nameBuffer.fill('\0');
		}
	}

	const bool removable = selected->name != domain::DEFAULT_PROFILE_NAME;

	if (design::Button(
		ImVec2(left + 152.0f, cursorY),
		text::profiles::DELETE_PROFILE,
		design::ButtonVariant::Failure,
		false,
		11.0f,
		removable
	)) {
		_notice.clear();

		const auto removed = profiles->Remove(selected->name);
		if (removed.has_value()) {
			state.SelectProfile(0);
		}
	}

	cursorY += 26.0f;

	design::TextAt(ImVec2(left, cursorY), profiles->LastMessage(), tokens::SECONDARY_MUTED, 10.0f);
	cursorY += 18.0f;

	if (selected->missingCount > 0) {
		design::TextAt(
			ImVec2(left, cursorY),
			std::format(text::profiles::MISSING_FORMAT, selected->missingCount),
			tokens::FAILURE,
			10.0f
		);
		cursorY += 18.0f;
	}

	design::TextAt(
		ImVec2(left, cursorY),
		selected->gameProfileHeld
		? text::profiles::PROFILE_FILE_HELD
		: text::profiles::PROFILE_FILE_ABSENT,
		selected->gameProfileHeld ? tokens::SUCCESS : tokens::TEXT_DISABLED,
		10.0f
	);

	if (!selected->account.empty()) {
		design::TextAt(
			ImVec2(left + 110.0f, cursorY),
			std::format(text::profiles::ACCOUNT_FORMAT, selected->account),
			tokens::SECONDARY_MUTED,
			10.0f
		);
	}

	cursorY += 20.0f;

	cursorY = DrawGameProfiles_(area, cursorY, profiles);

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::LOAD_ORDER,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const auto profile = profiles->Load(selected->name);
	if (!profile.has_value()) {
		design::TextAt(ImVec2(left, cursorY + 4.0f), text::profiles::UNREADABLE, tokens::FAILURE, 10.0f);
		return;
	}

	static const std::vector<domain::InstalledMod> NOTHING_INSTALLED;

	const std::vector<domain::InstalledMod>& installed = order != nullptr
	                                                     ? order->Current().installed
	                                                     : NOTHING_INSTALLED;

	std::size_t position = 0;

	for (const domain::OrderEntry& entry : profile->Order().Entries()) {
		if (!entry.enabled) {
			continue;
		}

		++position;

		const bool present = std::ranges::any_of(installed, [&](const domain::InstalledMod& mod) {
				return mod.folder == entry.folder;
			}
		);

		design::TextAt(
			ImVec2(left, cursorY + 4.0f),
			std::format(text::profiles::POSITION_FORMAT, position),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(left + 28.0f, cursorY + 4.0f),
			entry.folder.Value(),
			present ? tokens::SECONDARY : tokens::FAILURE,
			11.0f
		);

		if (!present) {
			design::TextAt(
				ImVec2(left + 240.0f, cursorY + 4.0f),
				text::profiles::NOT_INSTALLED,
				tokens::FAILURE,
				10.0f
			);
		}

		cursorY += DETAIL_ROW_HEIGHT;
		design::HorizontalRule(left, left + width, cursorY, tokens::BORDER_SUBTLE);
	}

	if (position == 0) {
		design::TextAt(ImVec2(left, cursorY + 4.0f), text::profiles::NOTHING_ENABLED, tokens::TEXT_DISABLED, 10.0f);
	}
}

void ProfilesScreen::Draw(
	const ScreenArea& area,
	ApplicationState& state,
	domain::IProfileService* profiles,
	domain::IOrderService* order
) {
	if (profiles == nullptr) {
		const float cursorY = ScreenToolbar::Draw(area, text::profiles::TITLE, text::profiles::UNAVAILABLE);
		ScreenToolbar::Placeholder(area, cursorY, text::common::NO_INSTALLATION);
		return;
	}

	const std::string context = profiles->Active().empty()
	                            ? std::string(text::profiles::NO_ACTIVE)
	                            : std::format(text::profiles::ACTIVE_FORMAT, profiles->Active());

	if (!_selectionPrimed && !profiles->Profiles().empty()) {
		_selectionPrimed = true;

		const std::vector<domain::ProfileSummary>& rows = profiles->Profiles();

		for (std::size_t index = 0; index < rows.size(); ++index) {
			if (rows[index].active) {
				state.SelectProfile(index);
				break;
			}
		}
	}

	const float cursorY = ScreenToolbar::Draw(area, text::profiles::TITLE, context);

	const float listBottom = DrawList_(area, cursorY, state, profiles);
	(void)listBottom;

	DrawDetail_(area, cursorY, state, profiles, order);
}
}
