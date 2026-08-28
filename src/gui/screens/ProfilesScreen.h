#pragma once

#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {

class ProfilesScreen {
public:
    static constexpr float ROW_HEIGHT = 34.0f;
    static constexpr float LIST_WIDTH = 236.0f;

    void Draw(const ScreenArea& area, ApplicationState& state);

private:
    [[nodiscard]] float DrawList_(const ScreenArea& area, float cursorY, ApplicationState& state) const;

    void DrawDetail_(const ScreenArea& area, float cursorY, ApplicationState& state) const;
};

}
