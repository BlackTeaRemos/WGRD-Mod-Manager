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
	[[nodiscard]] static std::optional<FetchRates> Refresh(
		FetchState& fetchState,
		std::uint64_t pendingWrites
	);

private:
	static void ApplyPriorities_(FetchState& fetchState, libtorrent::torrent_handle& handle);
};
}
