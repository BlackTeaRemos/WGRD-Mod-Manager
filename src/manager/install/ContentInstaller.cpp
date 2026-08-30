#include "manager/install/ContentInstaller.h"

#include "manager/io/MappedFile.h"

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

namespace wgrd::manager {
ContentInstaller::ContentInstaller(const domain::IContentHasher& hasher)
	: _hasher(&hasher) {}

std::expected<std::vector<std::byte>, InstallError> ContentInstaller::ResolveChunk_(
	const domain::ChunkPlacement& placement,
	const std::filesystem::path& modFolder,
	domain::IChunkSource& chunkSource
) const {
	if (placement.source == domain::ChunkSourceKind::Held) {
		const auto mapped = MappedFile::Open(modFolder / placement.heldPath);
		if (!mapped.has_value()) {
			return std::unexpected(InstallError::HeldChunkMissing);
		}

		const std::span<const std::byte> content = mapped->Data();
		if (placement.heldOffset + placement.length > content.size()) {
			return std::unexpected(InstallError::HeldChunkUnreadable);
		}

		const std::span<const std::byte> slice = content.subspan(placement.heldOffset, placement.length);
		return std::vector<std::byte>(slice.begin(), slice.end());
	}

	auto fetched = chunkSource.Fetch(placement.digest, placement.length);
	if (!fetched.has_value()) {
		return std::unexpected(InstallError::RemoteChunkUnavailable);
	}

	if (fetched->size() != placement.length) {
		return std::unexpected(InstallError::RemoteChunkCorrupt);
	}

	if (_hasher->Hash(*fetched) != placement.digest) {
		return std::unexpected(InstallError::RemoteChunkCorrupt);
	}

	return *fetched;
}

bool ContentInstaller::Swap_(
	const std::filesystem::path& staged,
	const std::filesystem::path& target
) {
	for (std::size_t attempt = 0; attempt < SWAP_ATTEMPTS; ++attempt) {
		std::error_code failure;
		std::filesystem::rename(staged, target, failure);

		if (!failure) {
			return true;
		}

		std::this_thread::sleep_for(SWAP_RETRY_DELAY);
	}

	return false;
}

bool ContentInstaller::AlreadySeeded_(
	const domain::FilePlan& file,
	const domain::ChunkPlacement& placement
) {
	return placement.source == domain::ChunkSourceKind::Held
	       && placement.heldPath == file.path
	       && placement.heldOffset == placement.targetOffset;
}

std::expected<void, InstallError> ContentInstaller::WriteStagedFile_(
	const domain::FilePlan& file,
	const std::filesystem::path& modFolder,
	domain::IChunkSource& chunkSource,
	const bool skipPlaced,
	InstallReport& report
) const {
	const std::filesystem::path target = modFolder / file.path;
	const std::filesystem::path staged = std::filesystem::path(target.string() + std::string(STAGING_SUFFIX));

	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	if (!std::filesystem::exists(staged, failure)) {
		std::ofstream created(staged, std::ios::binary | std::ios::trunc);
		if (!created) {
			return std::unexpected(InstallError::FolderUnwritable);
		}
	}

	failure.clear();
	std::filesystem::resize_file(staged, static_cast<std::uintmax_t>(file.size), failure);
	if (failure) {
		return std::unexpected(InstallError::FolderUnwritable);
	}

	std::fstream output(staged, std::ios::binary | std::ios::in | std::ios::out);
	if (!output) {
		return std::unexpected(InstallError::FolderUnwritable);
	}

	for (const domain::ChunkPlacement& placement : file.placements) {
		if (skipPlaced && placement.source == domain::ChunkSourceKind::Remote) {
			report.remoteBytes += placement.length;
			continue;
		}

		if (skipPlaced && AlreadySeeded_(file, placement)) {
			report.heldBytes += placement.length;
			continue;
		}

		const auto bytes = ResolveChunk_(placement, modFolder, chunkSource);
		if (!bytes.has_value()) {
			return std::unexpected(bytes.error());
		}

		output.seekp(static_cast<std::streamoff>(placement.targetOffset));
		output.write(
			reinterpret_cast<const char*>(bytes->data()),
			static_cast<std::streamsize>(bytes->size())
		);

		if (!output) {
			return std::unexpected(InstallError::WriteFailed);
		}

		if (placement.source == domain::ChunkSourceKind::Held) {
			report.heldBytes += placement.length;
		} else {
			report.remoteBytes += placement.length;
		}
	}

	output.close();
	if (!output) {
		return std::unexpected(InstallError::WriteFailed);
	}

	return {};
}

std::expected<InstallReport, InstallError> ContentInstaller::Apply(
	const domain::InstallPlan& plan,
	const std::filesystem::path& modFolder,
	domain::IChunkSource& chunkSource
) const {
	return Materialise_(plan, modFolder, chunkSource, false);
}

std::expected<InstallReport, InstallError> ContentInstaller::ApplyPlaced(
	const domain::InstallPlan& plan,
	const std::filesystem::path& modFolder,
	domain::IChunkSource& chunkSource
) const {
	return Materialise_(plan, modFolder, chunkSource, true);
}

std::expected<InstallReport, InstallError> ContentInstaller::Materialise_(
	const domain::InstallPlan& plan,
	const std::filesystem::path& modFolder,
	domain::IChunkSource& chunkSource,
	const bool skipPlaced
) const {
	std::error_code failure;
	std::filesystem::create_directories(modFolder, failure);
	if (failure) {
		return std::unexpected(InstallError::FolderUnwritable);
	}

	InstallReport report{0, 0, 0, 0};

	for (const domain::FilePlan& file : plan.Files()) {
		const auto written = WriteStagedFile_(file, modFolder, chunkSource, skipPlaced, report);
		if (!written.has_value()) {
			return std::unexpected(written.error());
		}
	}

	for (const domain::FilePlan& file : plan.Files()) {
		const std::filesystem::path target = modFolder / file.path;
		const std::filesystem::path staged =
				std::filesystem::path(target.string() + std::string(STAGING_SUFFIX));

		if (!Swap_(staged, target)) {
			return std::unexpected(InstallError::SwapFailed);
		}

		++report.filesWritten;
	}

	for (const std::string& removal : plan.Removals()) {
		if (std::filesystem::remove(modFolder / removal, failure)) {
			++report.filesRemoved;
		}
	}

	return report;
}
}
