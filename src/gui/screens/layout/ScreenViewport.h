#pragma once

#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {
class ScreenViewport {
public:
	[[nodiscard]] static ScreenArea Begin(const ScreenArea& area, Screen screen);
	static void End();
};
}
