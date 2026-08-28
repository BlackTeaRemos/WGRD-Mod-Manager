#include "manager/install/ManifestDiffer.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {
ManifestDiffer::ManifestDiffer() = default;

ManifestDiffer::~ManifestDiffer() = default;

ManifestDiffer::HeldChunkIndex ManifestDiffer::IndexHeldChunks_(const domain::ModManifest& held) {
	HeldChunkIndex index;

	for (const domain::ManifestFile& file : held.Files()) {
		for (const domain::ManifestChunk& chunk : file.chunks) {
			index.try_emplace(chunk.digest.ToHex(), HeldChunk{file.path, chunk.offset, chunk.length});
		}
	}

	return index;
}

domain::InstallPlan ManifestDiffer::Diff(
	const domain::ModManifest& held,
	const domain::ModManifest& target
) const {
	const HeldChunkIndex heldChunks = IndexHeldChunks_(held);

	std::vector<domain::FilePlan> files;
	files.reserve(target.Files().size());

	for (const domain::ManifestFile& file : target.Files()) {
		std::vector<domain::ChunkPlacement> placements;
		placements.reserve(file.chunks.size());

		for (const domain::ManifestChunk& chunk : file.chunks) {
			const auto match = heldChunks.find(chunk.digest.ToHex());

			if (match == heldChunks.end()) {
				placements.push_back(domain::ChunkPlacement{
						chunk.digest, chunk.offset, chunk.length, domain::ChunkSourceKind::Remote, std::string(), 0
					}
				);
				continue;
			}

			placements.push_back(domain::ChunkPlacement{
					chunk.digest, chunk.offset, chunk.length, domain::ChunkSourceKind::Held, match->second.path, match->second.offset
				}
			);
		}

		files.push_back(domain::FilePlan{file.path, file.size, std::move(placements)});
	}

	std::vector<std::string> removals;
	for (const domain::ManifestFile& file : held.Files()) {
		const bool retained = std::any_of(
			target.Files().begin(),
			target.Files().end(),
			[&file](const domain::ManifestFile& candidate) {
				return candidate.path == file.path;
			}
		);

		if (!retained) {
			removals.push_back(file.path);
		}
	}

	return domain::InstallPlan(std::move(files), std::move(removals));
}
}
