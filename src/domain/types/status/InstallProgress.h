#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wgrd::domain {
enum class InstallPhase {
	Idle
	, Planning
	, Fetching
	, Installing
	, Verifying
	, Done
	, Failed
};

struct InstallProgress {
	InstallPhase phase = InstallPhase::Idle;
	std::string identifier;
	std::string modName;
	std::uint64_t version = 0;
	std::uint64_t remoteBytes = 0;
	std::uint64_t heldBytes = 0;
	std::uint64_t fetchedBytes = 0;
	std::uint64_t inFlightBytes = 0;
	std::size_t remoteChunks = 0;
	std::uint32_t peers = 0;
	std::uint64_t hashFailures = 0;
	std::uint64_t bannedPeers = 0;
	std::string message;

	[[nodiscard]] bool Busy() const {
		return phase == InstallPhase::Planning
		       || phase == InstallPhase::Fetching
		       || phase == InstallPhase::Installing
		       || phase == InstallPhase::Verifying;
	}
};
}
