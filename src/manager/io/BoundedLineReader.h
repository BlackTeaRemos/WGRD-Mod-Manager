#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace wgrd::manager {
class BoundedLineReader {
public:
	[[nodiscard]] static std::optional<std::string> Read(
		const std::filesystem::path& path,
		std::size_t limit
	);

private:
	BoundedLineReader() = delete;
};
}
