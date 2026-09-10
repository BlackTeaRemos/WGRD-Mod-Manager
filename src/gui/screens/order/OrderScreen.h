#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IOrderHistory.h"
#include "domain/interfaces/order/IProfileService.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/order/OrderEntryList.h"
#include "gui/screens/order/OrderEditToolbar.h"

namespace wgrd::gui {
class OrderScreen {
public:
	static constexpr float BANNER_HEIGHT = 34.0f;
	void Draw(const ScreenArea& area, const ApplicationServices& services);

private:
	[[nodiscard]] static float DrawDefaultBanner_(const ScreenArea& area, float top, domain::IProfileService* profiles);
	void DrawSidebar_(const ScreenArea& area, float top, float right, const domain::OrderSnapshot& snapshot) const;
	[[nodiscard]] float DrawDetails_(float left, float top, float width, const domain::OrderSnapshot& snapshot) const;
	[[nodiscard]] static const domain::InstalledMod* InstalledFor_(const domain::OrderSnapshot& snapshot, const domain::InstallFolder& folder);

	OrderEntryList _entries;
	OrderEditToolbar _toolbar;
};
}
