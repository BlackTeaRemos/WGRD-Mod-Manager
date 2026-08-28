#include "manager/profile/GameProfileVault.h"

#include <array>
#include <fstream>
#include <system_error>

namespace wgrd::manager {
bool GameProfileVault::CarriesMagic(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return false;
	}

	std::array<char, 4> header{};
	input.read(header.data(), static_cast<std::streamsize>(header.size()));

	if (input.gcount() != static_cast<std::streamsize>(header.size())) {
		return false;
	}

	return std::string_view(header.data(), header.size()) == MAGIC;
}

std::expected<void, GameProfileVaultError> GameProfileVault::Verify_(
	const std::filesystem::path& source
) {
	std::error_code failure;

	if (!std::filesystem::is_regular_file(source, failure) || failure) {
		return std::unexpected(GameProfileVaultError::SourceMissing);
	}

	const std::uintmax_t size = std::filesystem::file_size(source, failure);
	if (failure || size > MAXIMUM_BYTES) {
		return std::unexpected(GameProfileVaultError::SourceOversized);
	}

	if (!CarriesMagic(source)) {
		return std::unexpected(GameProfileVaultError::SourceForeign);
	}

	return {};
}

std::expected<void, GameProfileVaultError> GameProfileVault::Capture(
	const std::filesystem::path& live,
	const std::filesystem::path& stored
) {
	const auto verified = Verify_(live);
	if (!verified.has_value()) {
		return verified;
	}

	std::error_code failure;
	std::filesystem::create_directories(stored.parent_path(), failure);

	std::filesystem::copy_file(
		live,
		stored,
		std::filesystem::copy_options::overwrite_existing,
		failure
	);

	if (failure) {
		return std::unexpected(GameProfileVaultError::CopyFailed);
	}

	return {};
}

std::expected<void, GameProfileVaultError> GameProfileVault::Restore(
	const std::filesystem::path& stored,
	const std::filesystem::path& live
) {
	const auto verified = Verify_(stored);
	if (!verified.has_value()) {
		return verified;
	}

	std::error_code failure;

	if (std::filesystem::is_regular_file(live, failure) && !failure) {
		const std::filesystem::path backup =
				live.string() + std::string(BACKUP_SUFFIX);

		std::filesystem::copy_file(
			live,
			backup,
			std::filesystem::copy_options::overwrite_existing,
			failure
		);

		if (failure) {
			return std::unexpected(GameProfileVaultError::BackupFailed);
		}
	}

	failure.clear();

	std::filesystem::copy_file(
		stored,
		live,
		std::filesystem::copy_options::overwrite_existing,
		failure
	);

	if (failure) {
		return std::unexpected(GameProfileVaultError::CopyFailed);
	}

	return {};
}
}
