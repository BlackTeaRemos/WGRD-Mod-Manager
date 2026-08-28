#pragma once

#include <filesystem>

namespace wgrd::domain {
struct GameInstallation {
	std::filesystem::path root;
	std::filesystem::path modsDirectory;
	std::filesystem::path orderFile;

	[[nodiscard]] bool IsUsable() const;
};
}
