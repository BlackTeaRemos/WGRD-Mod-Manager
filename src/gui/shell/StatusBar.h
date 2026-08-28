#pragma once

#include "gui/state/ApplicationServices.h"

#include <imgui.h>

namespace wgrd::gui {

class StatusBar {
public:
    void Draw(ImVec2 origin, float width, const ApplicationServices& services);
};

}
