#pragma once

#include <filesystem>
#include <optional>
#include "domain/types/content/ModPreview.h"

namespace wgrd::manager {
class PreviewImageReader {
public:
	[[nodiscard]] static std::optional<domain::ModPreview> Read(const std::filesystem::path& folder);
private:
	[[nodiscard]] static std::optional<std::vector<std::uint8_t>> ReadFile_(const std::filesystem::path& path, std::size_t limit);
};
}
