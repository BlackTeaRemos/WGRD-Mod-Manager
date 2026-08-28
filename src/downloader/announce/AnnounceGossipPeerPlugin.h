#pragma once

#include "domain/types/distribution/AnnounceSummary.h"
#include "downloader/announce/AnnounceExchange.h"

#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>

#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace wgrd::downloader {
class AnnounceGossipPeerPlugin final : public libtorrent::peer_plugin {
public:
	static constexpr std::string_view EXTENSION_NAME = "wgrd_announce";
	static constexpr int LOCAL_MESSAGE_ID = 111;
	static constexpr std::chrono::seconds REOFFER_INTERVAL{60};
	static constexpr std::chrono::seconds HOLDINGS_POLL_INTERVAL{2};

	AnnounceGossipPeerPlugin(
		libtorrent::peer_connection_handle connection,
		AnnounceExchange& exchange
	);

	~AnnounceGossipPeerPlugin() override;

	[[nodiscard]] libtorrent::string_view type() const override;

	void add_handshake(libtorrent::entry& handshake) override;

	bool on_extension_handshake(const libtorrent::bdecode_node& handshake) override;

	bool on_extended(int length, int message, libtorrent::span<const char> body) override;

	void tick() override;

private:
	[[nodiscard]] std::string PeerKey_() const;

	void Send_(const std::vector<std::uint8_t>& payload);

	void SendOffer_(std::vector<domain::AnnounceSummary> holdings);

	void HandleOffer_(libtorrent::span<const char> body);

	void HandleWant_(libtorrent::span<const char> body);

	void HandleRecord_(libtorrent::span<const char> body);

	[[nodiscard]] static std::string KeyOf_(const domain::AnnounceSummary& summary);

	libtorrent::peer_connection_handle _connection;
	AnnounceExchange* _exchange;
	int _remoteMessageId;
	bool _counted;
	std::set<std::string> _outstanding;
	std::vector<domain::AnnounceSummary> _lastOffered;
	std::chrono::steady_clock::time_point _lastOffer;
	std::chrono::steady_clock::time_point _lastHoldingsPoll;
};
}
