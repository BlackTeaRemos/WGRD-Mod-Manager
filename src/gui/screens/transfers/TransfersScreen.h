#pragma once

#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/IBaselineIndexService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/transfers/DownloadProgressPanel.h"

namespace wgrd::gui {
class TransfersScreen {
public:
	static constexpr float HELD_ROW_HEIGHT = 24.0f;
	void Draw(const ScreenArea& area, domain::ISwarmService* swarm,
		domain::ISeedingService* seeding, domain::IInstallService* install,
		domain::IAnnounceGossip* gossip, const domain::IBaselineIndexService* baseline);

private:
	[[nodiscard]] float DrawSwarmCard_(const ScreenArea& area, float top, domain::ISwarmService* swarm) const;
	[[nodiscard]] float DrawGossip_(const ScreenArea& area, float top, domain::IAnnounceGossip* gossip);
	[[nodiscard]] float DrawSeeding_(const ScreenArea& area, float top, domain::ISeedingService* seeding) const;
	DownloadProgressPanel _downloads;
};
}
