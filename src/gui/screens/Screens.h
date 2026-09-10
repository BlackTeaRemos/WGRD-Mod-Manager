#pragma once

#include "gui/screens/catalog/CatalogScreen.h"
#include "gui/screens/order/OrderScreen.h"
#include "gui/screens/profiles/ProfilesScreen.h"
#include "gui/screens/publish/PublishScreen.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/transfers/TransfersScreen.h"
#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"
#include "gui/settings/GameLocationControls.h"

namespace wgrd::gui {
class Screens {
public:
	void Draw(const ScreenArea& area, ApplicationState& state, const ApplicationServices& services);

private:
	GameLocationControls _location;
	CatalogScreen _catalog;
	OrderScreen _order;
	TransfersScreen _transfers;
	ProfilesScreen _profiles;
	PublishScreen _publish;
};
}
