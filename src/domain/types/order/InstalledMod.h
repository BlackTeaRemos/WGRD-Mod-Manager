#pragma once

#include "domain/types/content/ModMetadata.h"
#include "domain/types/game/GameBuild.h"
#include "domain/types/order/InstallFolder.h"

#include <vector>

namespace wgrd::domain {
struct InstalledMod {
	InstallFolder folder;
	std::vector<GameBuild> builds;
	ModMetadata metadata;

	[[nodiscard]] bool SupportsBuild(const GameBuild& build) const;
};
}
