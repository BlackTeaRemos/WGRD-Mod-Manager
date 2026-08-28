#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>

namespace wgrd::manager {
enum class GameProfileVaultError {
	SourceMissing
	, SourceOversized
	, SourceForeign
	, BackupFailed
	, CopyFailed
};

class GameProfileVault {
public:
	static constexpr std::string_view MAGIC = "ESAV";
	static constexpr std::string_view BACKUP_SUFFIX = ".wgrdbak";
	static constexpr std::uint64_t MAXIMUM_BYTES = 8ull * 1024 * 1024;

	[[nodiscard]] static bool CarriesMagic(const std::filesystem::path& path);

	[[nodiscard]] static std::expected<void, GameProfileVaultError> Capture(
		const std::filesystem::path& live,
		const std::filesystem::path& stored
	);

	[[nodiscard]] static std::expected<void, GameProfileVaultError> Restore(
		const std::filesystem::path& stored,
		const std::filesystem::path& live
	);

private:
	[[nodiscard]] static std::expected<void, GameProfileVaultError> Verify_(
		const std::filesystem::path& source
	);

	GameProfileVault() = delete;
};
}
