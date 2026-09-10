#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <string>

namespace wgrd::gui {
class GameLocationControls {
public:
	void Draw(ApplicationState& state, const ApplicationServices& services);

private:
	std::string _notice;
};
}
