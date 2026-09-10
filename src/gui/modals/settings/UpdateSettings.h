#pragma once

#include "domain/interfaces/services/IUpdateService.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {
class UpdateSettings {
public:
	static void Draw(ApplicationState& state, domain::IUpdateService* updates);
};
}
