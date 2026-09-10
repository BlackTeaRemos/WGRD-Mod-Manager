#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"
#include "gui/shell/support/SupportPanel.h"

#include <imgui.h>

namespace wgrd::gui {
class StatusBar {
public:
	void Draw(ImVec2 origin, float width, ApplicationState& state, const ApplicationServices& services);

private:
	[[nodiscard]] static float DrawManager_(ImVec2 origin, float cursor, bool detailed,
		ApplicationState& state, const ApplicationServices& services);
	[[nodiscard]] static float DrawPatcher_(ImVec2 origin, float cursor, bool detailed,
		const ApplicationServices& services);
	SupportPanel _support;
};
}
