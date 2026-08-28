#pragma once

#include "domain/types/game/GameInstallation.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace wgrd::manager {
class GameLocator {
public:
	[[nodiscard]] static std::optional<domain::GameInstallation> Resolve();

	[[nodiscard]] static std::vector<domain::GameInstallation> Detect();

	[[nodiscard]] static std::optional<domain::GameInstallation> FromRoot(const std::filesystem::path& root);

	[[nodiscard]] static std::optional<std::filesystem::path> SteamRoot();

private:
	static std::optional<std::filesystem::path> SteamRoot_();
	static std::vector<std::filesystem::path> LibraryRoots_(const std::filesystem::path& steamRoot);
};
}
