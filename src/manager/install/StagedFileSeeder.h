#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "domain/types/distribution/InstallPlan.h"

#include <filesystem>

namespace wgrd::manager {
class StagedFileSeeder {
public:
	explicit StagedFileSeeder(const domain::IContentHasher& hasher);

	[[nodiscard]] bool Seed(
		const std::filesystem::path& original,
		const std::filesystem::path& staged,
		const domain::FilePlan& file,
		bool& resumed
	) const;

private:
	[[nodiscard]] bool StagedMatchesHeldRanges_(
		const std::filesystem::path& staged,
		const domain::FilePlan& file
	) const;

	[[nodiscard]] static bool CopyFromOriginal_(
		const std::filesystem::path& original,
		const std::filesystem::path& staged
	);

	const domain::IContentHasher* _hasher;
};
}
