#include "gui/screens/CatalogScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CatalogText.h"
#include "gui/text/CommonText.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>

namespace wgrd::gui {
namespace {
	constexpr std::array<std::string_view, 3> FILTERS = {text::catalog::FILTER_ALL, text::catalog::FILTER_INSTALLED, text::catalog::FILTER_HELD};
}

bool CatalogScreen::Matches_(const domain::CatalogRow& row, const std::size_t filter) {
	if (filter == 1) {
		return row.installed;
	}
	if (filter == 2) {
		return row.manifestHeld;
	}
	return true;
}

void CatalogScreen::DrawRow_(
	const ScreenArea& area,
	const float cursorY,
	const domain::CatalogRow& row,
	ApplicationState& state,
	domain::IInstallService* install
) {
	const ImVec2 rowTopLeft(area.origin.x, cursorY);
	const ImVec2 rowBottomRight(area.origin.x + area.width, cursorY + ROW_HEIGHT);

	bool hovered = false;
	const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);

	if (hovered) {
		design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
	}

	design::TextAt(
		ImVec2(rowTopLeft.x + 6.0f, cursorY + 10.0f),
		row.installed ? text::catalog::INSTALLED_MARK : text::catalog::ABSENT_MARK,
		row.installed ? tokens::SUCCESS : tokens::TEXT_DISABLED,
		11.0f
	);

	design::TextAt(ImVec2(rowTopLeft.x + 26.0f, cursorY + 5.0f), row.modName, tokens::SECONDARY, 12.0f);
	design::TextAt(
		ImVec2(rowTopLeft.x + 26.0f, cursorY + 18.0f),
		row.publisher,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(rowTopLeft.x + 420.0f, cursorY + 10.0f),
		std::format(text::common::VERSION_FORMAT, row.version),
		tokens::SECONDARY,
		11.0f
	);

	if (row.manifestHeld) {
		design::TextAt(
			ImVec2(rowTopLeft.x + 500.0f, cursorY + 10.0f),
			ScreenToolbar::FormatBytes(row.totalBytes),
			tokens::SECONDARY,
			11.0f
		);

		design::TextAt(
			ImVec2(rowTopLeft.x + 600.0f, cursorY + 10.0f),
			std::format(text::catalog::CHUNKS_FORMAT, row.chunkCount),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(rowTopLeft.x + 700.0f, cursorY + 10.0f),
			std::format(text::catalog::FILES_FORMAT, row.fileCount),
			tokens::SECONDARY_MUTED,
			10.0f
		);
	} else {
		design::TextAt(
			ImVec2(rowTopLeft.x + 500.0f, cursorY + 10.0f),
			row.revoked ? text::catalog::REVOKED_NOTE : text::catalog::MANIFEST_ON_DEMAND,
			row.revoked ? tokens::FAILURE : tokens::ADVISORY,
			10.0f
		);
	}

	std::string_view stateLabel = row.installed
	                              ? text::catalog::STATE_INSTALLED
	                              : text::catalog::STATE_ANNOUNCED;

	ImU32 stateTone = row.installed ? tokens::SUCCESS : tokens::ACCENT;

	if (row.revoked) {
		stateLabel = text::catalog::STATE_UNSIGNED;
		stateTone = tokens::FAILURE;
	}

	design::TextAt(
		ImVec2(rowTopLeft.x + 790.0f, cursorY + 10.0f),
		stateLabel,
		stateTone,
		9.0f
	);

	if (!row.seedFault.empty()) {
		design::TextAt(
			ImVec2(rowTopLeft.x + 500.0f, cursorY + 22.0f),
			row.seedFault,
			tokens::FAILURE,
			9.0f
		);
	}

	ImVec2 actionTopLeft(rowBottomRight.x - 80.0f, cursorY + 6.0f);
	ImVec2 actionBottomRight(actionTopLeft.x, actionTopLeft.y);

	if (install != nullptr) {
		const bool outdated = row.Outdated();
		const bool current = row.VersionKnown() && !outdated;

		const std::string_view label = !row.installed
		                               ? text::catalog::ACTION_INSTALL
		                               : (outdated ? text::catalog::ACTION_UPDATE : text::catalog::ACTION_CURRENT);

		const ImVec2 size = design::ButtonSize(label);
		actionTopLeft = ImVec2(rowBottomRight.x - size.x - 8.0f, cursorY + 6.0f);
		actionBottomRight = ImVec2(actionTopLeft.x + size.x, actionTopLeft.y + size.y);

		const bool busy = install->Progress().Busy();
		const bool actionable = !busy && !current && !row.revoked;

		const design::ButtonVariant variant = actionable
		                                      ? design::ButtonVariant::Accent
		                                      : design::ButtonVariant::Neutral;

		if (design::Button(actionTopLeft, label, variant, actionable) && actionable) {
			const auto started = install->Start(row.identifier);
			(void)started;
		}
	}

	design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

	if (design::RowSecondaryHit(rowTopLeft, rowBottomRight)) {
		OpenMenu_(row, ImGui::GetIO().MousePos);
		return;
	}

	if (clicked && !design::PointerInside(actionTopLeft, actionBottomRight)) {
		state.OpenDetail(row.identifier);
	}
}

void CatalogScreen::OpenMenu_(const domain::CatalogRow& row, const ImVec2 at) {
	_menuIdentifier = row.identifier;
	_menuModName = row.modName;
	_menuInstalled = row.installed;
	_menuConfirming = false;
	_menuOpen = true;
	_menuOrigin = at;
	_notice.clear();
}

void CatalogScreen::CloseMenu_() {
	_menuOpen = false;
	_menuConfirming = false;
	_menuIdentifier.clear();
	_menuModName.clear();
}

void CatalogScreen::DrawContextMenu_(
	const ScreenArea& area,
	domain::ICatalogService* catalog,
	domain::IModRemovalService* removal,
	domain::IInstallService* install
) {
	if (!_menuOpen) {
		return;
	}

	constexpr float height = MENU_ITEM_HEIGHT * 2.0f + 8.0f;

	const float left = std::min(_menuOrigin.x, area.origin.x + area.width - MENU_WIDTH - 4.0f);
	const float top = std::min(_menuOrigin.y, area.origin.y + area.height - height - 4.0f);

	const ImVec2 origin(left, top);
	const ImVec2 bottomRight(origin.x + MENU_WIDTH, origin.y + height);

	design::Shadow(origin, bottomRight);
	design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);
	design::StrokeRect(origin, bottomRight, tokens::BORDER);

	design::TextAt(
		ImVec2(origin.x + 8.0f, origin.y + 5.0f),
		_menuModName,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	const ImVec2 itemTopLeft(origin.x + 1.0f, origin.y + MENU_ITEM_HEIGHT);
	const ImVec2 itemBottomRight(bottomRight.x - 1.0f, itemTopLeft.y + MENU_ITEM_HEIGHT);

	const bool busy = install != nullptr && install->Progress().Busy();
	const bool removable = _menuInstalled && removal != nullptr && !busy;

	bool itemHovered = false;
	const bool itemClicked = design::RowHit(itemTopLeft, itemBottomRight, itemHovered);

	if (removable && itemHovered) {
		design::FillRect(itemTopLeft, itemBottomRight, tokens::ACCENT_HOVER_FILL);
	}

	const std::string_view itemLabel = !_menuInstalled
	                                   ? text::catalog::MENU_NOT_INSTALLED
	                                   : (busy ? text::catalog::MENU_BUSY : (_menuConfirming ? text::catalog::MENU_CONFIRM : text::catalog::MENU_DELETE));

	design::TextAt(
		ImVec2(itemTopLeft.x + 8.0f, itemTopLeft.y + 5.0f),
		itemLabel,
		removable ? (_menuConfirming ? tokens::FAILURE : tokens::SECONDARY) : tokens::TEXT_DISABLED,
		11.0f
	);

	if (itemClicked && removable) {
		if (!_menuConfirming) {
			_menuConfirming = true;
			return;
		}

		const auto removed = removal->Remove(_menuIdentifier);
		_notice = removal->LastMessage();

		if (removed.has_value() && catalog != nullptr) {
			catalog->Refresh();
		}

		CloseMenu_();
		return;
	}

	const bool outsideClick =
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			&& !design::PointerInside(origin, bottomRight);

	if (outsideClick) {
		CloseMenu_();
	}
}

void CatalogScreen::Draw(
	const ScreenArea& area,
	ApplicationState& state,
	domain::ICatalogService* catalog,
	domain::IInstallService* install,
	domain::IModRemovalService* removal
) {
	if (catalog == nullptr) {
		const float cursorY = ScreenToolbar::Draw(area, text::catalog::TITLE, text::catalog::UNAVAILABLE);
		ScreenToolbar::Placeholder(area, cursorY, text::catalog::NO_DATA_DIRECTORY);
		return;
	}

	const std::vector<domain::CatalogRow>& rows = catalog->Rows();

	const std::string context = std::format(
		text::catalog::CONTEXT_FORMAT,
		rows.size(),
		catalog->RegisteredKeys(),
		catalog->RejectedCount()
	);

	float cursorY = ScreenToolbar::Draw(area, text::catalog::TITLE, context);

	float filterX = area.origin.x + 6.0f;
	const float filterY = cursorY + 5.0f;

	for (std::size_t index = 0; index < FILTERS.size(); ++index) {
		const bool active = state.CatalogFilter() == index;
		const ImVec2 size = design::ButtonSize(FILTERS[index]);

		if (design::Button(
			ImVec2(filterX, filterY),
			FILTERS[index],
			active ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral,
			active
		)) {
			state.SetCatalogFilter(index);
		}

		filterX += size.x + 6.0f;
	}

	if (design::Button(ImVec2(filterX + 12.0f, filterY), text::catalog::REFRESH, design::ButtonVariant::Neutral)) {
		catalog->Refresh();
		return;
	}

	cursorY += 28.0f;

	design::HorizontalRule(area.origin.x, area.origin.x + area.width, cursorY, tokens::BORDER);
	design::TextAt(ImVec2(area.origin.x + 26.0f, cursorY + 4.0f), text::catalog::COLUMN_MOD, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(ImVec2(area.origin.x + 420.0f, cursorY + 4.0f), text::catalog::COLUMN_VERSION, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(ImVec2(area.origin.x + 500.0f, cursorY + 4.0f), text::catalog::COLUMN_SIZE, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(ImVec2(area.origin.x + 600.0f, cursorY + 4.0f), text::catalog::COLUMN_CHUNKS, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(ImVec2(area.origin.x + 700.0f, cursorY + 4.0f), text::catalog::COLUMN_FILES, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(ImVec2(area.origin.x + 790.0f, cursorY + 4.0f), text::catalog::COLUMN_STATE, tokens::SECONDARY_MUTED, 9.0f);
	cursorY += 16.0f;

	std::size_t shown = 0;
	for (const domain::CatalogRow& row : rows) {
		if (!Matches_(row, state.CatalogFilter())) {
			continue;
		}

		DrawRow_(area, cursorY, row, state, install);
		cursorY += ROW_HEIGHT;
		++shown;
	}

	if (shown == 0) {
		ScreenToolbar::Placeholder(area, cursorY, text::catalog::EMPTY);
	}

	if (!_notice.empty()) {
		design::TextAt(
			ImVec2(area.origin.x + 6.0f, area.origin.y + area.height - 16.0f),
			_notice,
			tokens::SECONDARY_MUTED,
			10.0f
		);
	}

	DrawContextMenu_(area, catalog, removal, install);
}
}
