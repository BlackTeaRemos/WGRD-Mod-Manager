#pragma once

#include "gui/state/ApplicationState.h"

#include <imgui.h>

namespace wgrd::gui {

class NavigationRail {
public:
    void Draw(ImVec2 origin, float height, ApplicationState& state);
};

}
