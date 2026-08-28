#pragma once

#include "domain/types/status/UpdateStatus.h"

#include <string>

namespace wgrd::domain {
struct PatcherStatus {
	UpdatePhase phase = UpdatePhase::Idle;
	bool present = false;
	std::string installedTag;
	std::string runtimeStamp;
	std::string latestTag;
	std::string message;

	[[nodiscard]] bool Busy() const {
		return phase == UpdatePhase::Checking || phase == UpdatePhase::Downloading;
	}
};
}
