#pragma once

#include "domain/interfaces/services/IInstallService.h"
#include "gui/design/Primitives.h"
#include "gui/screens/ScreenArea.h"

#include <string>

namespace wgrd::gui {
class DownloadProgressPanel {
public:
	void Draw(const ScreenArea& area, float top, domain::IInstallService* install);

private:
	static constexpr std::uint64_t PENDING_WRITE_BLOCK_BYTES = 16384;
	static constexpr float DOWNLOAD_BAR_HEIGHT = 18.0f;
	static constexpr float DOWNLOAD_BAR_INSET = 6.0f;
	static constexpr float DOWNLOAD_BAR_TOP_OFFSET = 38.0f;
	[[nodiscard]] static design::TransferSegments SegmentsFor_(const domain::InstallProgress& progress);
	std::string _retryError;
};
}
