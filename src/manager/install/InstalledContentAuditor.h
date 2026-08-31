#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace wgrd::manager {
class InstalledContentAuditor {
public:
	struct Audit {
		std::vector<domain::FilePlan> damagedFiles;
		std::uint64_t verifiedBytes = 0;
		std::size_t damagedChunks = 0;
	};

	explicit InstalledContentAuditor(const domain::IContentHasher& hasher);

	[[nodiscard]] Audit Examine(
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		const std::function<void(std::uint64_t)>& onVerified
	) const;

private:
	const domain::IContentHasher* _hasher;
};
}
