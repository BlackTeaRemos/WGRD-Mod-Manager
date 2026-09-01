#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::downloader {
class SeedingSwitch {
public:
	static constexpr std::string_view FILE_NAME = "seeding.state";
	static constexpr std::string_view PAUSED_MARK = "paused";
	static constexpr std::string_view RUNNING_MARK = "running";

	SeedingSwitch();

	void UseFolder(std::filesystem::path folder);

	[[nodiscard]] std::optional<bool> Load() const;

	void Save(bool enabled) const;

private:
	std::filesystem::path _folder;
};
}
