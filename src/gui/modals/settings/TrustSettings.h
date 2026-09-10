#pragma once

#include "domain/interfaces/trust/IRegistryUpdater.h"

namespace wgrd::gui {
class TrustSettings {
public:
	static void Draw(domain::IRegistryUpdater* registry);
};
}
