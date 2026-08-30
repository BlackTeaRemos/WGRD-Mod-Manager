#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
enum class PublishPhase {
	Idle
	, Hashing
	, Signing
	, Announcing
	, Done
	, Failed
};

struct PublishProgress {
	PublishPhase phase = PublishPhase::Idle;
	std::string modName;
	std::uint64_t processedBytes = 0;
	std::uint64_t totalBytes = 0;
	std::string message;

	[[nodiscard]] bool Busy() const {
		return phase == PublishPhase::Hashing
		       || phase == PublishPhase::Signing
		       || phase == PublishPhase::Announcing;
	}
};
}
