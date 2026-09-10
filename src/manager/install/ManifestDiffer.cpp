#include "manager/install/ManifestDiffer.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {
ManifestDiffer::ManifestDiffer() = default;

ManifestDiffer::~ManifestDiffer() = default;

ManifestDiffer::HeldChunkIndex ManifestDiffer::IndexHeldChunks_(
	const domain::ModManifest& held,
	const std::filesystem::path& heldRoot
) {
	HeldChunkIndex index;

	for (const domain::ManifestFile& file : held.Files()) {
		for (const domain::ManifestChunk& chunk : file.chunks) {
			index.try_emplace(
				chunk.digest.ToHex(),
				domain::HeldChunkLocation{heldRoot / file.path, chunk.offset, chunk.length}
			);
		}
	}

	return index;
}

bool ManifestDiffer::FileUnchanged_(
	const domain::ModManifest& held,
	const domain::ManifestFile& file
) {
	const auto match = std::find_if(
		held.Files().begin(),
		held.Files().end(),
		[&file](const domain::ManifestFile& candidate) {
			return candidate.path == file.path;
		}
	);

	return match != held.Files().end() && *match == file;
}

domain::ChunkPlacement ManifestDiffer::Place_(
	const domain::ManifestChunk& chunk,
	const HeldChunkIndex& heldChunks,
	const domain::IHeldChunkCatalogue& catalogue
) {
	const auto match = heldChunks.find(chunk.digest.ToHex());

	if (match != heldChunks.end()) {
		return domain::ChunkPlacement{
			chunk.digest, chunk.offset, chunk.length, domain::ChunkSourceKind::Held, match->second.file, match->second.offset
		};
	}

	const std::optional<domain::HeldChunkLocation> catalogued = catalogue.Locate(chunk.digest);

	if (catalogued.has_value() && catalogued->length == chunk.length) {
		return domain::ChunkPlacement{
			chunk.digest, chunk.offset, chunk.length, domain::ChunkSourceKind::Held, catalogued->file, catalogued->offset
		};
	}

	return domain::ChunkPlacement{
		chunk.digest, chunk.offset, chunk.length, domain::ChunkSourceKind::Remote, std::filesystem::path(), 0
	};
}

domain::InstallPlan ManifestDiffer::Diff(
	const domain::ModManifest& held,
	const std::filesystem::path& heldRoot,
	const domain::ModManifest& target,
	const domain::IHeldChunkCatalogue& catalogue
) const {
	const HeldChunkIndex heldChunks = IndexHeldChunks_(held, heldRoot);

	std::vector<domain::FilePlan> files;
	files.reserve(target.Files().size());

	for (const domain::ManifestFile& file : target.Files()) {
		if (FileUnchanged_(held, file)) {
			continue;
		}

		std::vector<domain::ChunkPlacement> placements;
		placements.reserve(file.chunks.size());

		for (const domain::ManifestChunk& chunk : file.chunks) {
			placements.push_back(Place_(chunk, heldChunks, catalogue));
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
