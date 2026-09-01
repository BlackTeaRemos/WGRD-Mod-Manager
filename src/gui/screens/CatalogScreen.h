#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/IModRemovalService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <string>

namespace wgrd::gui {
class CatalogScreen {
public:
	static constexpr float ROW_HEIGHT = 32.0f;
	static constexpr float MENU_WIDTH = 168.0f;
	static constexpr float MENU_ITEM_HEIGHT = 22.0f;

	void Draw(
		const ScreenArea& area,
		ApplicationState& state,
		domain::ICatalogService* catalog,
		domain::IInstallService* install,
		domain::IModRemovalService* removal,
		domain::IAnnounceGossip* gossip
	);

private:
	[[nodiscard]] static bool Matches_(const domain::CatalogRow& row, std::size_t filter);

	void DrawRow_(
		const ScreenArea& area,
		float cursorY,
		const domain::CatalogRow& row,
		ApplicationState& state,
		domain::IInstallService* install
	);

	void DrawContextMenu_(
		const ScreenArea& area,
		domain::ICatalogService* catalog,
		domain::IModRemovalService* removal,
		domain::IInstallService* install
	);

	void OpenMenu_(const domain::CatalogRow& row, ImVec2 at);

	void CloseMenu_();

	std::string _menuIdentifier;
	std::string _menuModName;
	bool _menuInstalled = false;
	bool _menuConfirming = false;
	bool _menuOpen = false;
	ImVec2 _menuOrigin{};
	std::string _notice;
};
}
