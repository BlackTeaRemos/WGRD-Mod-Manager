#include "manager/install/FetchPlanner.h"

#include "manager/install/StagedFileSeeder.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <set>

namespace wgrd::manager {
FetchPlanner::FetchPlanner(const domain::IContentHasher& hasher)
	: _hasher(&hasher) {}

std::optional<FetchPlanner::Request> FetchPlanner::Stage(
	const domain::InstallPlan& plan,
	const std::filesystem::path& modFolder,
	const std::string_view stagingSuffix
) const {
	const StagedFileSeeder seeder(*_hasher);

	Request request;
	std::set<std::string> seen;

	for (const domain::FilePlan& file : plan.Files()) {
		const std::filesystem::path original = modFolder / file.path;
		const std::filesystem::path staged =
				std::filesystem::path(original.string() + std::string(stagingSuffix));

		if (!seeder.Seed(original, staged, file, request.resumed)) {
			return std::nullopt;
		}

		for (const domain::ChunkPlacement& placement : file.placements) {
			if (placement.source != domain::ChunkSourceKind::Remote) {
				continue;
			}

			const std::string chunkFileName = domain::ChunkFileNaming::FileNameFor(placement.digest);

			if (seen.insert(placement.digest.ToHex()).second) {
				request.wantedFiles.push_back(chunkFileName);
			}

			request.destinations.push_back(domain::ChunkDestination{
					chunkFileName, staged, placement.targetOffset, placement.length
				}
			);
		}
	}

	return request;
}
}
