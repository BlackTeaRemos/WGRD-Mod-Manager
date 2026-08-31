#pragma once

#include "downloader/transfer/FetchState.h"

#include <optional>

namespace wgrd::downloader {
struct FetchRates {
	int downloadRate;
	int uploadRate;
};

class FetchRefresher {
public:
	[[nodiscard]] static std::optional<FetchRates> Refresh(FetchState& fetchState, bool writesSettled);

private:
	static void ApplyPriorities_(FetchState& fetchState, libtorrent::torrent_handle& handle);
};
}
