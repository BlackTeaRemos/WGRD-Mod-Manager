#pragma once

#include "domain/types/distribution/AnnounceSummary.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace wgrd::domain {
class IAnnounceCatalogue {
public:
	virtual ~IAnnounceCatalogue() = 0;

	[[nodiscard]] virtual std::vector<AnnounceSummary> Summaries() const = 0;

	[[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> Record(
		const PublisherFingerprint& publisher,
		std::string_view modName
	) const = 0;

	[[nodiscard]] virtual bool WouldAccept(
		const PublisherFingerprint& publisher,
		std::string_view modName,
		std::uint64_t version
	) const = 0;
};

inline IAnnounceCatalogue::~IAnnounceCatalogue() = default;
}
