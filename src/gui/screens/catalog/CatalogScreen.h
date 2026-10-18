#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/IModRemovalService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/catalog/CatalogSort.h"
#include "gui/state/ApplicationState.h"

#include <array>
#include <string>
#include <set>

namespace wgrd::gui {
class CatalogScreen {
public:
	void Draw(const ScreenArea& area, ApplicationState& state, domain::ICatalogService* catalog,
		domain::IInstallService* install, domain::IModRemovalService* removal, domain::IAnnounceGossip* gossip);

private:
	[[nodiscard]] float DrawControls_(const ScreenArea& area, float top, ApplicationState& state,
		domain::ICatalogService& catalog, domain::IAnnounceGossip* gossip);
	[[nodiscard]] float DrawNotice_(const ScreenArea& area, float top, ApplicationState& state,
		domain::IInstallService* install);
	void Start_(domain::IInstallService& install, const std::string& identifier);
	void DrawMenus_(domain::ICatalogService& catalog, domain::IModRemovalService* removal,
		domain::IInstallService* install);

	std::array<char, 192> _search{};
	CatalogSort _sort = CatalogSort::Name;
	bool _focusSearch = false;
	std::string _menuIdentifier;
	std::string _menuModName;
	std::string _menuPublisher;
	bool _menuInstalled = false;
	std::string _notice;
	std::string _retryIdentifier;
	bool _noticeError = false;
	std::set<std::string> _previewRequests;
};
}
