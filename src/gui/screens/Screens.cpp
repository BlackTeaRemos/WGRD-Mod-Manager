#include "gui/screens/Screens.h"

#include "gui/screens/layout/ScreenViewport.h"
#include "gui/screens/ScreenToolbar.h"

namespace wgrd::gui {
void Screens::Draw(
	const ScreenArea& area,
	ApplicationState& state,
	const ApplicationServices& services
) {
	const ScreenArea content = ScreenViewport::Begin(area, state.ActiveScreen());
	if (services.order == nullptr && services.gameLocation != nullptr) {
		const float top = ScreenToolbar::Draw(content, "Game location", "Game folder missing");
		ImGui::SetCursorScreenPos(ImVec2(content.origin.x + 6.0f, top + 14.0f));
		_location.Draw(state, services);
		ScreenViewport::End();
		return;
	}
	switch (state.ActiveScreen()) {
		case Screen::Catalog:
			_catalog.Draw(content, state, services.catalog, services.install, services.removal, services.gossip);
			break;
		case Screen::Order:
			_order.Draw(content, services);
			break;
		case Screen::Transfers:
			_transfers.Draw(content, services.swarm, services.seeding, services.install, services.gossip, services.baseline);
			break;
		case Screen::Profiles:
			_profiles.Draw(content, state, services.profiles, services.order);
			break;
		case Screen::Publish:
			_publish.Draw(content, state, services.publish, services.catalog, services.files);
			break;
	}
	ScreenViewport::End();
}
}
