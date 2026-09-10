#include "gui/screens/profiles/ProfilesScreen.h"

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
	return ImVec2(area.origin.x + (area.width < 950.0f ? 0.0f : LIST_WIDTH + 1.0f), cursorY);
}

float ProfilesScreen::HeadingWidth_(const ScreenArea& area) {
	return area.width < 950.0f ? area.width : area.width - LIST_WIDTH - 1.0f;
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
		const ImVec2 rowBottomRight(area.origin.x + (area.width < 950.0f ? area.width : LIST_WIDTH), rowY + ROW_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
		const bool focused = ImGui::IsItemFocused();
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
			12.0f, FontRole::Name
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

		if (clicked || focused) {
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
	const float left = area.origin.x + (area.width < 950.0f ? 6.0f : LIST_WIDTH + 12.0f);
	const float width = area.width - (area.width < 950.0f ? 12.0f : LIST_WIDTH + 18.0f);

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::GAME_PROFILES,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const std::vector<domain::GameProfileFile> discovered = profiles->Discovered();

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
			_operationFailed = false;

			const auto assigned = profiles->SetDefault(candidate);
			_operationFailed = !assigned.has_value();
		}

		cursorY += DETAIL_ROW_HEIGHT;
		design::HorizontalRule(left, left + width, cursorY, tokens::BORDER_SUBTLE);
	}

	return cursorY + 8.0f;
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
	DrawDetail_(area, area.width < 950.0f ? listBottom + 12.0f : cursorY, state, profiles, order);
}
}
