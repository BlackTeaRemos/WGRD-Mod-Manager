#pragma once

#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "gui/design/Primitives.h"
#include "gui/screens/ScreenArea.h"


namespace wgrd::gui {
class TransfersScreen {
public:
	static constexpr float HELD_ROW_HEIGHT = 24.0f;
	static constexpr std::uint64_t PENDING_WRITE_BLOCK_BYTES = 16384;
	static constexpr float DOWNLOAD_BAR_HEIGHT = 18.0f;
	static constexpr float DOWNLOAD_BAR_INSET = 6.0f;
	static constexpr float DOWNLOAD_BAR_TOP_OFFSET = 38.0f;

	void Draw(
		const ScreenArea& area,
		domain::ISwarmService* swarm,
		domain::ISeedingService* seeding,
		domain::IInstallService* install,
		domain::IAnnounceGossip* gossip
	);

private:
	[[nodiscard]] float DrawSwarmCard_(
		const ScreenArea& area,
		float cursorY,
		domain::ISwarmService* swarm
	) const;

	[[nodiscard]] float DrawGossip_(
		const ScreenArea& area,
		float cursorY,
		domain::IAnnounceGossip* gossip
	);

	[[nodiscard]] static design::TransferSegments SegmentsFor_(
		const domain::InstallProgress& progress
	);

	void DrawDownloads_(
		const ScreenArea& area,
		float cursorY,
		domain::IInstallService* install
	) const;

	[[nodiscard]] float DrawSeeding_(
		const ScreenArea& area,
		float cursorY,
		domain::ISeedingService* seeding
	) const;
};
}
