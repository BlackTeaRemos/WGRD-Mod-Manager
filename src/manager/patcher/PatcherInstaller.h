#pragma once

#include <expected>
#include <filesystem>
#include <string_view>

namespace wgrd::manager {
enum class PatcherInstallError {
	GameMissing
	, StagedMissing
	, StagedEmpty
	, SystemLibraryMissing
	, ProxyWriteFailed
	, ForwardWriteFailed
	, ModsFolderFailed
};

class PatcherInstaller {
public:
	static constexpr std::string_view PROXY_NAME = "version.dll";
	static constexpr std::string_view FORWARD_NAME = "version_real.dll";
	static constexpr std::string_view MODS_FOLDER = "mods";
	static constexpr std::string_view GAME_EXECUTABLE = "WarGame3.exe";

	[[nodiscard]] static std::filesystem::path ProxyPathIn(const std::filesystem::path& gameRoot);

	[[nodiscard]] static bool Installed(const std::filesystem::path& gameRoot);

	[[nodiscard]] static std::filesystem::path SystemLibrary();

	[[nodiscard]] static std::expected<void, PatcherInstallError> Install(
		const std::filesystem::path& gameRoot,
		const std::filesystem::path& stagedProxy
	);

private:
	PatcherInstaller() = delete;
};
}
