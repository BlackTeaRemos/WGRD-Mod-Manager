#include "downloader/transfer/AlertRouter.h"

#include <libtorrent/alert_types.hpp>

namespace wgrd::downloader {
AlertOutcome AlertRouter::Route(
	const std::vector<libtorrent::alert*>& alerts,
	FetchState& fetchState
) {
	AlertOutcome outcome;

	const std::optional<libtorrent::torrent_handle> active = fetchState.Active();

	const auto fetchOwns = [&active](const libtorrent::torrent_handle& handle) {
		return active.has_value() && handle == *active;
	};

	for (const libtorrent::alert* const entry : alerts) {
		if (const auto* const stats = libtorrent::alert_cast<libtorrent::dht_stats_alert>(entry)) {
			std::uint64_t nodes = 0;
			for (const libtorrent::dht_routing_bucket& bucket : stats->routing_table) {
				nodes += static_cast<std::uint64_t>(bucket.num_nodes);
			}
			outcome.dhtNodes = nodes;
			continue;
		}

		if (const auto* const removed =
				libtorrent::alert_cast<libtorrent::torrent_removed_alert>(entry)) {
			if (fetchState.ConfirmRemoval(removed->handle)) {
				outcome.clearDestinations = true;
			}
			continue;
		}

		if (const auto* const failedHash =
				libtorrent::alert_cast<libtorrent::hash_failed_alert>(entry)) {
			if (fetchOwns(failedHash->handle)) {
				fetchState.CountHashFailure(PEER_SENT_BAD_DATA);
			}
			outcome.lastTransferFailure = std::string(PEER_SENT_BAD_DATA);
			continue;
		}

		if (const auto* const banned =
				libtorrent::alert_cast<libtorrent::peer_ban_alert>(entry)) {
			if (fetchOwns(banned->handle)) {
				fetchState.CountBannedPeer(PEER_BANNED_BAD_DATA);
			}
			outcome.lastTransferFailure = std::string(PEER_BANNED_BAD_DATA);
			continue;
		}

		if (const auto* const unreadable =
				libtorrent::alert_cast<libtorrent::file_error_alert>(entry)) {
			if (fetchOwns(unreadable->handle)) {
				fetchState.Fail(FETCH_STORAGE_FAILED);
			}
			outcome.lastTransferFailure = unreadable->error.message();
			continue;
		}

		if (const auto* const broken =
				libtorrent::alert_cast<libtorrent::torrent_error_alert>(entry)) {
			if (fetchOwns(broken->handle)) {
				fetchState.Fail(FETCH_TORRENT_FAILED);
			}
			outcome.lastTransferFailure = broken->error.message();
			continue;
		}

		if (const auto* const rejected =
				libtorrent::alert_cast<libtorrent::metadata_failed_alert>(entry)) {
			if (fetchOwns(rejected->handle)) {
				fetchState.Fail(METADATA_REJECTED);
			}
			continue;
		}

		if (const auto* const dropped =
				libtorrent::alert_cast<libtorrent::peer_disconnected_alert>(entry)) {
			const bool refused =
					dropped->error == boost::asio::error::connection_refused ||
					dropped->error == boost::asio::error::timed_out;

			const std::string described = dropped->message();
			if (!refused && described.find(LOOPBACK_ADDRESS) != std::string::npos) {
				outcome.lastPeerError = dropped->error.message();
			}
			continue;
		}

		if (const auto* const failed =
				libtorrent::alert_cast<libtorrent::peer_error_alert>(entry)) {
			outcome.lastPeerError = failed->error.message();
		}
	}

	return outcome;
}
}
