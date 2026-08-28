#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
struct SwarmStatus {
	bool running = false;
	bool dhtRunning = false;
	std::uint64_t dhtNodes = 0;
	std::uint32_t listenPort = 0;
	std::string listenInterface;
	std::uint64_t downloadRate = 0;
	std::uint64_t uploadRate = 0;
};

class ISwarmService {
public:
	virtual ~ISwarmService() = 0;

	[[nodiscard]] virtual const SwarmStatus& Status() const = 0;

	virtual void Poll() = 0;
};

inline ISwarmService::~ISwarmService() = default;
}
