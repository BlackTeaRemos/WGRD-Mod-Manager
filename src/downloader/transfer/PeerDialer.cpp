#include "downloader/transfer/PeerDialer.h"

#include <libtorrent/address.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <algorithm>

namespace wgrd::downloader {
PeerDialer::PeerDialer()
	: _guard()
	, _manualPeers()
	, _neighbourDials(0) {}

bool PeerDialer::AddManualPeer(const std::string_view address, const std::uint16_t port) {
	if (port == 0) {
		return false;
	}

	libtorrent::error_code parsing;
	libtorrent::make_address(std::string(address), parsing);

	if (parsing) {
		return false;
	}

	const std::pair<std::string, std::uint16_t> peer{std::string(address), port};

	const std::scoped_lock lock(_guard);

	if (std::ranges::find(_manualPeers, peer) == _manualPeers.end()) {
		_manualPeers.push_back(peer);
	}

	return true;
}

void PeerDialer::DialManual(libtorrent::torrent_handle& handle) const {
	if (!handle.is_valid()) {
		return;
	}

	std::vector<std::pair<std::string, std::uint16_t>> peers;

	{
		const std::scoped_lock lock(_guard);
		peers = _manualPeers;
	}

	for (const auto& peer : peers) {
		libtorrent::error_code parsing;
		const libtorrent::address parsed = libtorrent::make_address(peer.first, parsing);

		if (parsing) {
			continue;
		}

		handle.connect_peer(libtorrent::tcp::endpoint(parsed, peer.second));
	}
}

void PeerDialer::DialNeighbours(libtorrent::torrent_handle& handle, const std::uint16_t ownPort) {
	if (!handle.is_valid()) {
		return;
	}

	const libtorrent::address loopback = libtorrent::make_address_v4("127.0.0.1");

	std::uint64_t dialled = 0;

	for (int offset = 0; offset < PORT_RETRY_LIMIT; ++offset) {
		const auto candidate = static_cast<std::uint16_t>(NEIGHBOUR_BASE_PORT + offset);

		if (candidate == ownPort) {
			continue;
		}

		handle.connect_peer(libtorrent::tcp::endpoint(loopback, candidate));
		++dialled;
	}

	const std::scoped_lock lock(_guard);
	_neighbourDials += dialled;
}

std::uint64_t PeerDialer::NeighbourDials() const {
	const std::scoped_lock lock(_guard);
	return _neighbourDials;
}
}
