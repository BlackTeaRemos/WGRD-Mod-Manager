#include "domain/types/order/InstalledMod.h"

#include <algorithm>

namespace wgrd::domain {
bool InstalledMod::SupportsBuild(const GameBuild& build) const {
	return std::ranges::find(builds, build) != builds.end();
}
}
