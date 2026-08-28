#pragma once

#include "domain/types/identity/PublisherFingerprint.h"

#include <cstdint>
#include <string>

namespace wgrd::domain {
struct AnnounceSummary {
	PublisherFingerprint publisher;
	std::string modName;
	std::uint64_t version;

	[[nodiscard]] std::string Identifier() const {
		return publisher.ToHex() + "/" + modName;
	}

	bool operator==(const AnnounceSummary& other) const = default;
};
}
