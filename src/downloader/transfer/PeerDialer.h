#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace libtorrent {
struct torrent_handle;
}

namespace wgrd::downloader {
class PeerDialer {
public:
	static constexpr int PORT_RETRY_LIMIT = 20;
	static constexpr std::uint16_t NEIGHBOUR_BASE_PORT = 6881;

	explicit PeerDialer();

	PeerDialer(const PeerDialer&) = delete;

	PeerDialer& operator=(const PeerDialer&) = delete;

	[[nodiscard]] bool AddManualPeer(std::string_view address, std::uint16_t port);

	void DialManual(libtorrent::torrent_handle& handle) const;

	void DialNeighbours(libtorrent::torrent_handle& handle, std::uint16_t ownPort);

	[[nodiscard]] std::uint64_t NeighbourDials() const;

private:
	mutable std::mutex _guard;
	std::vector<std::pair<std::string, std::uint16_t>> _manualPeers;
	std::uint64_t _neighbourDials;
};
}
