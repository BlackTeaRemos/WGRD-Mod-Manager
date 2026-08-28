#pragma once

#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "gui/screens/ScreenArea.h"

namespace wgrd::gui {

class TransfersScreen {
public:
    static constexpr float HELD_ROW_HEIGHT = 24.0f;

    void Draw(
        const ScreenArea& area,
        domain::ISwarmService* swarm,
        domain::ISeedingService* seeding,
        domain::IInstallService* install);

private:
    [[nodiscard]] float DrawSwarmCard_(
        const ScreenArea& area,
        float cursorY,
        domain::ISwarmService* swarm) const;

    void DrawDownloads_(
        const ScreenArea& area,
        float cursorY,
        domain::IInstallService* install) const;

    [[nodiscard]] float DrawSeeding_(
        const ScreenArea& area,
        float cursorY,
        domain::ISeedingService* seeding) const;
};

}
