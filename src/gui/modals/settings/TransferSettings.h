#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {
class TransferSettings {
public:
	static void Draw(ApplicationState& state, const ApplicationServices& services);
};
}
