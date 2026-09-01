#pragma once

#include "domain/types/status/GossipStatus.h"

#include <cstdint>
#include <string_view>

namespace wgrd::domain {
class IAnnounceGossip {
public:
	virtual ~IAnnounceGossip() = 0;

	[[nodiscard]] virtual const GossipStatus& Gossip() const = 0;

	[[nodiscard]] virtual bool AddGossipPeer(std::string_view address, std::uint16_t port) = 0;

	virtual void RequestGossipRefresh() = 0;
};

inline IAnnounceGossip::~IAnnounceGossip() = default;
}
