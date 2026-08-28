#pragma once

#include "gui/screens/CatalogScreen.h"
#include "gui/screens/OrderScreen.h"
#include "gui/screens/ProfilesScreen.h"
#include "gui/screens/PublishScreen.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/TransfersScreen.h"
#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {
class Screens {
public:
	void Draw(const ScreenArea& area, ApplicationState& state, const ApplicationServices& services);

private:
	CatalogScreen _catalog;
	OrderScreen _order;
	TransfersScreen _transfers;
	ProfilesScreen _profiles;
	PublishScreen _publish;
};
}
