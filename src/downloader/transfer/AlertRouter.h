#pragma once

#include "downloader/transfer/FetchState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace libtorrent {
struct alert;
}

namespace wgrd::downloader {
struct AlertOutcome {
	std::optional<std::uint64_t> dhtNodes;
	bool clearDestinations = false;
	std::optional<std::string> lastPeerError;
	std::optional<std::string> lastTransferFailure;
	std::uint64_t trackerReplies = 0;
	std::uint64_t trackerPeers = 0;
	std::uint64_t trackerFailures = 0;
	std::optional<std::string> lastTrackerError;
};

class AlertRouter {
public:
	static constexpr std::string_view LOOPBACK_ADDRESS = "127.0.0.1";
	static constexpr std::string_view PEER_SENT_BAD_DATA = "peer sent bad data";
	static constexpr std::string_view PEER_BANNED_BAD_DATA = "peer banned bad data";
	static constexpr std::string_view FETCH_TORRENT_FAILED = "fetch torrent failed";
	static constexpr std::string_view FETCH_STORAGE_FAILED = "fetch storage failed";
	static constexpr std::string_view METADATA_REJECTED = "metadata rejected";

	[[nodiscard]] static AlertOutcome Route(
		const std::vector<libtorrent::alert*>& alerts,
		FetchState& fetchState
	);
};
}
