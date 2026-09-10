#pragma once

#include "domain/types/content/FileStamp.h"

#include <filesystem>
#include <optional>

namespace wgrd::manager {
class FileStampReader {
public:
	[[nodiscard]] static std::optional<domain::FileStamp> Read(const std::filesystem::path& file);
};
}
