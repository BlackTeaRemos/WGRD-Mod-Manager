#include "gui/screens/catalog/CatalogScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/feedback/InstallStartText.h"
#include "gui/modals/ModalSurface.h"
#include "gui/screens/catalog/CatalogQuery.h"
#include "gui/screens/catalog/CatalogRowView.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CatalogText.h"

#include <algorithm>
#include <format>

namespace wgrd::gui {
void CatalogScreen::Start_(domain::IInstallService& install, const std::string& identifier) {
	const auto started = install.Start(identifier);
	_noticeError = !started.has_value();
	_notice = started.has_value() ? "Download started" : std::string(InstallStartText::Describe(started.error()));
	_retryIdentifier = started.has_value() ? std::string() : identifier;
}

float CatalogScreen::DrawControls_(const ScreenArea& area, float top, ApplicationState& state,
	domain::ICatalogService& catalog, domain::IAnnounceGossip* gossip) {
	constexpr std::array<std::string_view, 3> FILTERS = {"All", "Installed", "Manifest held"};
	float left = area.origin.x + 6.0f;
	for (std::size_t filter = 0; filter < FILTERS.size(); ++filter) {
		const ImVec2 size = design::ButtonSize(FILTERS[filter]);
		if (left + size.x > area.origin.x + area.width - 6.0f) {
			left = area.origin.x + 6.0f;
			top += 24.0f;
		}
		const bool active = state.CatalogFilter() == filter;
		if (design::Button(ImVec2(left, top), FILTERS[filter], active ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral, active)) {
			state.SetCatalogFilter(filter);
		}
		left += size.x + 6.0f;
	}
	if (left + design::ButtonSize("Refresh").x > area.origin.x + area.width - 6.0f) {
		left = area.origin.x + 6.0f;
		top += 24.0f;
	}
	if (design::Button(ImVec2(left, top), "Refresh", design::ButtonVariant::Neutral)) {
		if (gossip != nullptr) {
			gossip->RequestGossipRefresh();
		}
		catalog.Refresh();
	}
	top += 26.0f;
	design::TextAt(ImVec2(area.origin.x + 6.0f, top), "Search mods", tokens::SECONDARY_MUTED, 9.0f);
	top += 14.0f;
	const bool modalOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (!modalOpen && (_focusSearch || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteGlobal))) {
		ImGui::SetKeyboardFocusHere();
		_focusSearch = false;
	}
	design::TextField(ImVec2(area.origin.x + 6.0f, top), area.width - 12.0f, "catalog_search", _search.data(), _search.size());
	if (!modalOpen && ImGui::IsKeyPressed(ImGuiKey_Slash, false) && !ImGui::GetIO().WantTextInput) {
		_focusSearch = true;
	}
	top += 30.0f;
	constexpr std::array<std::string_view, 3> SORTS = {"Name", "Size", "Updates"};
	left = area.origin.x + 6.0f;
	for (std::size_t position = 0; position < SORTS.size(); ++position) {
		const CatalogSort sort = static_cast<CatalogSort>(position);
		const bool active = _sort == sort;
		if (design::Button(ImVec2(left, top), SORTS[position], active ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral, active)) {
			_sort = sort;
		}
		left += design::ButtonSize(SORTS[position]).x + 6.0f;
	}
	return top + 26.0f;
}

float CatalogScreen::DrawNotice_(const ScreenArea& area, const float top, ApplicationState& state,
	domain::IInstallService* install) {
	if (_notice.empty()) {
		return top;
	}
	design::ClippedText::Draw(ImVec2(area.origin.x + 6.0f, top), area.width - 12.0f, _notice,
		_noticeError ? tokens::FAILURE : tokens::SUCCESS, 11.0f, FontRole::Body);
	float left = area.origin.x + 6.0f;
	const float buttons = top + 18.0f;
	if (_noticeError) {
		if (design::Button(ImVec2(left, buttons), "Copy error", design::ButtonVariant::Neutral)) {
			ImGui::SetClipboardText(_notice.c_str());
		}
		left += design::ButtonSize("Copy error").x + 6.0f;
		if (!_retryIdentifier.empty() && install != nullptr && !install->Progress().Busy()) {
			if (design::Button(ImVec2(left, buttons), "Retry", design::ButtonVariant::Accent)) {
				const std::string identifier = _retryIdentifier;
				Start_(*install, identifier);
			}
		}
	} else if (install != nullptr && install->Progress().Busy()) {
		if (design::Button(ImVec2(left, buttons), "View transfer", design::ButtonVariant::Neutral)) {
			state.SetScreen(Screen::Transfers);
		}
	}
	return buttons + 25.0f;
}

void CatalogScreen::DrawMenus_(domain::ICatalogService& catalog, domain::IModRemovalService* removal,
	domain::IInstallService* install) {
	bool confirm = false;
	if (ModalSurface::BeginMenu("Mod actions", 220.0f)) {
		if (ImGui::MenuItem("Copy identifier")) {
			ImGui::SetClipboardText(_menuIdentifier.c_str());
		}
		if (ImGui::MenuItem("Copy fingerprint")) {
			ImGui::SetClipboardText(_menuIdentifier.substr(0, _menuIdentifier.find('/')).c_str());
		}
		const bool busy = install != nullptr && install->Progress().Busy();
		if (ImGui::MenuItem("Delete from disk", nullptr, false, _menuInstalled && removal != nullptr && !busy)) {
			confirm = true;
		}
		ModalSurface::End();
	}
	if (confirm) {
		ImGui::OpenPopup("Delete mod");
	}
	if (ModalSurface::Begin("Delete mod", nullptr, 340.0f)) {
		ImGui::TextUnformatted("Delete installed files");
		ImGui::TextUnformatted(_menuModName.c_str());
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		const bool allowed = removal != nullptr && (install == nullptr || !install->Progress().Busy());
		ImGui::BeginDisabled(!allowed);
		if (ImGui::Button("Delete")) {
			const auto removed = removal->Remove(_menuIdentifier);
			_notice = removal->LastMessage();
			_noticeError = !removed.has_value();
			_retryIdentifier.clear();
			if (removed.has_value()) {
				catalog.Refresh();
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ModalSurface::End();
	}
}

void CatalogScreen::Draw(const ScreenArea& area, ApplicationState& state, domain::ICatalogService* catalog,
	domain::IInstallService* install, domain::IModRemovalService* removal, domain::IAnnounceGossip* gossip) {
	if (catalog == nullptr) {
		const float top = ScreenToolbar::Draw(area, text::catalog::TITLE, text::catalog::UNAVAILABLE);
		ScreenToolbar::Placeholder(area, top, text::catalog::NO_DATA_DIRECTORY);
		return;
	}

	const std::string context = std::format("{} mods", catalog->Rows().size());
	float top = ScreenToolbar::Draw(area, text::catalog::TITLE, context) + 6.0f;
	top = DrawControls_(area, top, state, *catalog, gossip);
	top = DrawNotice_(area, top, state, install);
	CatalogRowView::Header(area, top);
	top += 17.0f;

	const auto rows = CatalogQuery::Select(catalog->Rows(), _search.data(), state.CatalogFilter(), _sort);
	for (const domain::CatalogRow* row : rows) {
		ImGui::PushID(row->identifier.c_str());
		const CatalogRowAction action = CatalogRowView::Draw(area, top, *row, install != nullptr,
			install != nullptr && install->Progress().Busy());
		ImGui::PopID();
		if (action == CatalogRowAction::Install && install != nullptr) {
			Start_(*install, row->identifier);
		} else if (action == CatalogRowAction::Details) {
			state.OpenDetail(row->identifier);
		} else if (action == CatalogRowAction::ContextMenu) {
			_menuIdentifier = row->identifier;
			_menuModName = row->modName;
			_menuPublisher = row->publisher;
			_menuInstalled = row->installed;
			ImGui::OpenPopup("Mod actions");
		}
		top += CatalogRowView::HEIGHT;
	}
	if (rows.empty()) {
		ScreenToolbar::Placeholder(area, top, _search[0] == '\0' ? "No mods available" : "No matching mods");
	}
	DrawMenus_(*catalog, removal, install);
}
}
