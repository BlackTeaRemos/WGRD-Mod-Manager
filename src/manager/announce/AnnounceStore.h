#pragma once

#include "domain/types/distribution/SignedAnnounce.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace wgrd::manager {
class AnnounceStore {
public:
	static constexpr std::string_view FILE_EXTENSION = ".wgrda";
	static constexpr std::size_t RECORD_BYTES = 220;
	static constexpr std::size_t MAXIMUM_RECORDS = 4096;

	explicit AnnounceStore(std::filesystem::path folder);

	~AnnounceStore();

	[[nodiscard]] bool Save(
		const domain::SignedAnnounce& announce,
		std::span<const std::uint8_t> record
	) const;

	[[nodiscard]] std::vector<std::vector<std::uint8_t>> LoadAll() const;

private:
	[[nodiscard]] std::filesystem::path PathFor_(const domain::SignedAnnounce& announce) const;

	std::filesystem::path _folder;
};
}
