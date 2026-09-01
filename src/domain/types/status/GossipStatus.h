#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
struct GossipStatus {
	bool running = false;
	bool controlValid = false;
	std::uint32_t peers = 0;
	std::uint32_t controlPeers = 0;
	std::uint32_t controlState = 0;
	std::uint64_t neighbourDials = 0;
	std::uint64_t offersSent = 0;
	std::uint64_t offersReceived = 0;
	std::uint64_t recordsSent = 0;
	std::uint64_t recordsReceived = 0;
	std::uint64_t recordsAccepted = 0;
	std::uint64_t recordsRejected = 0;
	std::uint64_t peersThrottled = 0;
	std::uint64_t protocolViolations = 0;
	std::string lastPeerError;

	bool operator==(const GossipStatus& other) const = default;
};
}
