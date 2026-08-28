#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
enum class UpdatePhase {
	Idle
	, Checking
	, UpToDate
	, Available
	, Downloading
	, Ready
	, Failed
};

struct UpdateStatus {
	UpdatePhase phase = UpdatePhase::Idle;
	std::string currentVersion;
	std::string latestVersion;
	std::string message;
	std::uint64_t downloadedBytes = 0;
	std::uint64_t totalBytes = 0;

	[[nodiscard]] bool Busy() const {
		return phase == UpdatePhase::Checking || phase == UpdatePhase::Downloading;
	}
};
}
