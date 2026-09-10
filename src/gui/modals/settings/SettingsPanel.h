#pragma once

#include "gui/settings/GameLocationControls.h"

#include <string>

namespace wgrd::gui {
class SettingsPanel {
public:
	void Draw(ApplicationState& state, const ApplicationServices& services);

private:
	GameLocationControls _location;
	std::string _notice;
};
}
