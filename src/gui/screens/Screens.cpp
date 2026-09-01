#include "gui/screens/Screens.h"

namespace wgrd::gui {
void Screens::Draw(
	const ScreenArea& area,
	ApplicationState& state,
	const ApplicationServices& services
) {
	switch (state.ActiveScreen()) {
		case Screen::Catalog:
			_catalog.Draw(area, state, services.catalog, services.install, services.removal, services.gossip);
			return;
		case Screen::Order:
			_order.Draw(area, services.order, services.profiles, services.catalog);
			return;
		case Screen::Transfers:
			_transfers.Draw(area, services.swarm, services.seeding, services.install, services.gossip);
			return;
		case Screen::Profiles:
			_profiles.Draw(area, state, services.profiles, services.order);
			return;
		case Screen::Publish:
			_publish.Draw(area, state, services.publish, services.catalog, services.files);
	}
}
}
