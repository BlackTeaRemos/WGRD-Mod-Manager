#pragma once

#include "gui/platform/Win32Window.h"
#include "gui/state/ApplicationState.h"

#include <imgui.h>

namespace wgrd::gui {

class TitleBar {
public:
    void Draw(ImVec2 origin, float width, ApplicationState& state, Win32Window& window);

private:
    bool _dragging = false;
    ImVec2 _dragOrigin{};
};

}
