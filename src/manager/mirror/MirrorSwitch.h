#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::manager {
class MirrorSwitch {
public:
	static constexpr std::string_view FILE_NAME = "mirror.state";
	static constexpr std::string_view MIRRORING_MARK = "mirroring";
	static constexpr std::string_view IDLE_MARK = "idle";

	explicit MirrorSwitch(std::filesystem::path folder);

	[[nodiscard]] std::optional<bool> Load() const;

	void Save(bool enabled) const;

private:
	std::filesystem::path _folder;
};
}
