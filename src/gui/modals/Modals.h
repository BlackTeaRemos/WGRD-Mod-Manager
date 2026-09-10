#pragma once

#include "gui/modals/details/ModDetailsPanel.h"
#include "gui/modals/settings/SettingsPanel.h"

namespace wgrd::gui {
class Modals {
public:
	void Draw(ApplicationState& state, const ApplicationServices& services);

private:
	SettingsPanel _settings;
	ModDetailsPanel _details;
};
}
