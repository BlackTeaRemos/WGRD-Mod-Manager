#pragma once

#include "manager/baseline/BaselineFileEntry.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class BaselineFileScanner {
public:
	static constexpr std::size_t FILE_LIMIT = 4096;

	[[nodiscard]] static std::vector<BaselineFileEntry> Scan(const std::filesystem::path& packRoot);

private:
	[[nodiscard]] static bool IsPack_(const std::filesystem::path& file);
};
}
