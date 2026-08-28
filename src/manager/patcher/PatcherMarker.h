#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace wgrd::manager {
class PatcherMarker {
public:
	static constexpr std::string_view FILE_NAME = "patcher_release";
	static constexpr std::size_t TAG_LIMIT = 64;

	explicit PatcherMarker(std::filesystem::path dataDirectory);

	~PatcherMarker();

	[[nodiscard]] std::string Read() const;

	[[nodiscard]] bool Write(std::string_view tag) const;

private:
	[[nodiscard]] std::filesystem::path PathFor_() const;

	std::filesystem::path _dataDirectory;
};
}
