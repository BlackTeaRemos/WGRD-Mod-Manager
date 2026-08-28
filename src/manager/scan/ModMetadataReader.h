#pragma once

#include "domain/types/content/ModMetadata.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace wgrd::manager {
class ModMetadataReader {
public:
	static constexpr std::string_view FILE_NAME = "mod.json";
	static constexpr std::uint64_t MAXIMUM_BYTES = 256 * 1024;
	static constexpr std::size_t FIELD_LIMIT = 4096;

	[[nodiscard]] static domain::ModMetadata Read(const std::filesystem::path& modDirectory);

private:
	[[nodiscard]] static std::string Field_(const void* document, std::string_view key);

	ModMetadataReader() = delete;
};
}
