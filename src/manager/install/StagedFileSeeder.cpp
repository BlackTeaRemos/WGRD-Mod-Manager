#include "manager/install/StagedFileSeeder.h"

#include "manager/io/MappedFile.h"

#include <fstream>
#include <system_error>

namespace wgrd::manager {
StagedFileSeeder::StagedFileSeeder(const domain::IContentHasher& hasher)
	: _hasher(&hasher) {}

bool StagedFileSeeder::StagedMatchesHeldRanges_(
	const std::filesystem::path& staged,
	const domain::FilePlan& file
) const {
	const auto mapped = MappedFile::Open(staged);
	if (!mapped.has_value()) {
		return false;
	}

	const std::span<const std::byte> content = mapped->Data();

	for (const domain::ChunkPlacement& placement : file.placements) {
		if (placement.source != domain::ChunkSourceKind::Held) {
			continue;
		}

		if (placement.heldPath != file.path || placement.heldOffset != placement.targetOffset) {
			continue;
		}

		if (placement.targetOffset + placement.length > content.size()) {
			return false;
		}

		const std::span<const std::byte> range = content.subspan(
			placement.targetOffset,
			placement.length
		);

		if (_hasher->Hash(range) != placement.digest) {
			return false;
		}
	}

	return true;
}

bool StagedFileSeeder::CopyFromOriginal_(
	const std::filesystem::path& original,
	const std::filesystem::path& staged
) {
	std::error_code failure;

	if (std::filesystem::is_regular_file(original, failure) && !failure) {
		std::filesystem::copy_file(
			original,
			staged,
			std::filesystem::copy_options::overwrite_existing,
			failure
		);

		if (!failure) {
			return true;
		}
	}

	std::ofstream created(staged, std::ios::binary | std::ios::trunc);

	return static_cast<bool>(created);
}

bool StagedFileSeeder::Seed(
	const std::filesystem::path& original,
	const std::filesystem::path& staged,
	const domain::FilePlan& file,
	bool& resumed
) const {
	std::error_code failure;
	std::filesystem::create_directories(staged.parent_path(), failure);

	if (failure) {
		return false;
	}

	if (std::filesystem::is_regular_file(staged, failure) && !failure) {
		if (StagedMatchesHeldRanges_(staged, file)) {
			resumed = true;
		} else {
			failure.clear();
			std::filesystem::remove(staged, failure);

			if (!CopyFromOriginal_(original, staged)) {
				return false;
			}
		}
	} else if (!CopyFromOriginal_(original, staged)) {
		return false;
	}

	failure.clear();
	std::filesystem::resize_file(staged, static_cast<std::uintmax_t>(file.size), failure);

	return !failure;
}
}
