#pragma once

#include "domain/interfaces/content/IChunkSource.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/types/distribution/InstallPlan.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

namespace wgrd::manager {
enum class InstallError {
	FolderUnwritable
	, HeldChunkMissing
	, HeldChunkUnreadable
	, RemoteChunkUnavailable
	, RemoteChunkCorrupt
	, WriteFailed
	, SwapFailed
};

struct InstallReport {
	std::uint64_t heldBytes;
	std::uint64_t remoteBytes;
	std::size_t filesWritten;
	std::size_t filesRemoved;
};

class ContentInstaller {
public:
	static constexpr std::string_view STAGING_SUFFIX = ".wgrdmm-new";

	explicit ContentInstaller(const domain::IContentHasher& hasher);

	[[nodiscard]] std::expected<InstallReport, InstallError> Apply(
		const domain::InstallPlan& plan,
		const std::filesystem::path& modFolder,
		domain::IChunkSource& chunkSource
	) const;

	[[nodiscard]] std::expected<InstallReport, InstallError> ApplyPlaced(
		const domain::InstallPlan& plan,
		const std::filesystem::path& modFolder,
		domain::IChunkSource& chunkSource
	) const;

private:
	[[nodiscard]] std::expected<InstallReport, InstallError> Materialise_(
		const domain::InstallPlan& plan,
		const std::filesystem::path& modFolder,
		domain::IChunkSource& chunkSource,
		bool skipPlaced
	) const;

	[[nodiscard]] static bool AlreadySeeded_(
		const domain::FilePlan& file,
		const domain::ChunkPlacement& placement
	);

	[[nodiscard]] std::expected<std::vector<std::byte>, InstallError> ResolveChunk_(
		const domain::ChunkPlacement& placement,
		const std::filesystem::path& modFolder,
		domain::IChunkSource& chunkSource
	) const;

	[[nodiscard]] std::expected<void, InstallError> WriteStagedFile_(
		const domain::FilePlan& file,
		const std::filesystem::path& modFolder,
		domain::IChunkSource& chunkSource,
		bool skipPlaced,
		InstallReport& report
	) const;

	const domain::IContentHasher* _hasher;
};
}
