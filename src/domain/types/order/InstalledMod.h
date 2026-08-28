#pragma once

#include "domain/types/game/GameBuild.h"
#include "domain/types/order/InstallFolder.h"

#include <vector>

namespace wgrd::domain {

struct InstalledMod {
    InstallFolder folder;
    std::vector<GameBuild> builds;

    [[nodiscard]] bool SupportsBuild(const GameBuild& build) const;
};

}
