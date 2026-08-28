#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace wgrd::manager {
enum class ManifestStoreError {
	DigestRejected
	, Unwritable
	, NotHeld
	, Unreadable
	, TooLarge
};

class ManifestStore {
public:
	static constexpr std::string_view ENVELOPE_SUFFIX = ".wgrdm";

	explicit ManifestStore(std::filesystem::path folder);

	[[nodiscard]] std::expected<std::filesystem::path, ManifestStoreError> Save(
		std::string_view digestHex,
		std::span<const std::uint8_t> sealed
	) const;

	[[nodiscard]] std::expected<std::vector<std::uint8_t>, ManifestStoreError> Load(
		std::string_view digestHex
	) const;

	[[nodiscard]] bool Holds(std::string_view digestHex) const;

	[[nodiscard]] std::filesystem::path PathFor(std::string_view digestHex) const;

private:
	[[nodiscard]] static bool IsDigestHex_(std::string_view digestHex);

	[[nodiscard]] std::filesystem::path PathFor_(std::string_view digestHex) const;

	std::filesystem::path _folder;
};
}
