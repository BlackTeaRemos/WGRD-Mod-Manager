#include "manager/patcher/PatcherInstaller.h"

#include <Windows.h>

#include <array>
#include <string>
#include <system_error>

namespace wgrd::manager {
namespace {
	std::filesystem::path WindowsDirectory() {
		std::array<wchar_t, MAX_PATH> buffer{};

		const UINT written = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
		if (written == 0 || written >= buffer.size()) {
			return {};
		}

		return std::filesystem::path(std::wstring(buffer.data(), written));
	}
}

std::filesystem::path PatcherInstaller::ProxyPathIn(const std::filesystem::path& gameRoot) {
	return gameRoot / std::string(PROXY_NAME);
}

bool PatcherInstaller::Installed(const std::filesystem::path& gameRoot) {
	std::error_code failure;
	return std::filesystem::is_regular_file(ProxyPathIn(gameRoot), failure) && !failure;
}

std::filesystem::path PatcherInstaller::SystemLibrary() {
	const std::filesystem::path windows = WindowsDirectory();
	if (windows.empty()) {
		return {};
	}

	std::error_code failure;

	const std::filesystem::path wow = windows / "SysWOW64" / std::string(PROXY_NAME);
	if (std::filesystem::is_regular_file(wow, failure) && !failure) {
		return wow;
	}

	failure.clear();

	const std::filesystem::path native = windows / "System32" / std::string(PROXY_NAME);
	if (std::filesystem::is_regular_file(native, failure) && !failure) {
		return native;
	}

	return {};
}

std::expected<void, PatcherInstallError> PatcherInstaller::Install(
	const std::filesystem::path& gameRoot,
	const std::filesystem::path& stagedProxy
) {
	std::error_code failure;

	if (!std::filesystem::is_regular_file(gameRoot / std::string(GAME_EXECUTABLE), failure) || failure) {
		return std::unexpected(PatcherInstallError::GameMissing);
	}

	failure.clear();

	if (!std::filesystem::is_regular_file(stagedProxy, failure) || failure) {
		return std::unexpected(PatcherInstallError::StagedMissing);
	}

	if (std::filesystem::file_size(stagedProxy, failure) == 0 || failure) {
		return std::unexpected(PatcherInstallError::StagedEmpty);
	}

	const std::filesystem::path system = SystemLibrary();
	if (system.empty()) {
		return std::unexpected(PatcherInstallError::SystemLibraryMissing);
	}

	failure.clear();

	std::filesystem::copy_file(
		system,
		gameRoot / std::string(FORWARD_NAME),
		std::filesystem::copy_options::overwrite_existing,
		failure
	);

	if (failure) {
		return std::unexpected(PatcherInstallError::ForwardWriteFailed);
	}

	failure.clear();

	std::filesystem::copy_file(
		stagedProxy,
		ProxyPathIn(gameRoot),
		std::filesystem::copy_options::overwrite_existing,
		failure
	);

	if (failure) {
		return std::unexpected(PatcherInstallError::ProxyWriteFailed);
	}

	failure.clear();

	std::filesystem::create_directories(gameRoot / std::string(MODS_FOLDER), failure);
	if (failure) {
		return std::unexpected(PatcherInstallError::ModsFolderFailed);
	}

	return {};
}
}
