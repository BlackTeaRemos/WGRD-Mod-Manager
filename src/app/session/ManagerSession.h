#pragma once

#include "domain/interfaces/platform/IGameLocation.h"
#include "domain/types/game/GameInstallation.h"

#include <optional>

namespace wgrd::app {
class ManagerSession {
public:
	ManagerSession(std::optional<domain::GameInstallation> installation, domain::IGameLocation& location);
	[[nodiscard]] int Run();

private:
	std::optional<domain::GameInstallation> _installation;
	domain::IGameLocation* _location;
};
}
