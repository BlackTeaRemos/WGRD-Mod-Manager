#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace wgrd::manager {
class PatcherRuntimeVersion {
public:
	static constexpr std::string_view FILE_NAME = "patcher_version.txt";
	static constexpr std::size_t LENGTH_LIMIT = 64;

	[[nodiscard]] static std::string Read(const std::filesystem::path& modsFolder);

private:
	[[nodiscard]] static bool Printable_(std::string_view text);

	PatcherRuntimeVersion() = delete;
};
}
