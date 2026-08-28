#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <imgui.h>

namespace wgrd::gui {

class Modals {
public:
    void Reserve(ImVec2 frameOrigin, float frameWidth, const ApplicationState& state) const;

    void Draw(ImVec2 frameOrigin, float frameWidth, ApplicationState& state, const ApplicationServices& services);

private:
    void DrawSettings_(ImVec2 frameOrigin, float frameWidth, ApplicationState& state, const ApplicationServices& services);
    float DrawTrust_(ImVec2 origin, float cursorY, domain::IRegistryUpdater* updater) const;
    float DrawUpdates_(ImVec2 origin, float cursorY, ApplicationState& state, domain::IUpdateService* updates) const;
    void DrawDetail_(ImVec2 frameOrigin, float frameWidth, ApplicationState& state);

    bool _bittorrent = true;
    bool _chunking = true;
    bool _seedAfterInstall = true;
    bool _seedMultiple = false;
    bool _mirrorAll = false;
    bool _httpsMirrors = false;
};

}
