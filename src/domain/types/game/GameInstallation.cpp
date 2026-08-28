#include "domain/types/game/GameInstallation.h"

#include <system_error>

namespace wgrd::domain {
bool GameInstallation::IsUsable() const {
	std::error_code probe;
	return !root.empty() && std::filesystem::is_directory(modsDirectory, probe) && !probe;
}
}
