#pragma once

#include "domain/interfaces/content/IHeldChunkCatalogue.h"
#include "domain/interfaces/content/IManifestDiffer.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"

#include <filesystem>
#include <map>
#include <string>

namespace wgrd::manager {
class ManifestDiffer final : public domain::IManifestDiffer {
public:
	ManifestDiffer();

	~ManifestDiffer() override;

	[[nodiscard]] domain::InstallPlan Diff(
		const domain::ModManifest& held,
		const std::filesystem::path& heldRoot,
		const domain::ModManifest& target,
		const domain::IHeldChunkCatalogue& catalogue
	) const override;

private:
	using HeldChunkIndex = std::map<std::string, domain::HeldChunkLocation>;

	[[nodiscard]] static HeldChunkIndex IndexHeldChunks_(
		const domain::ModManifest& held,
		const std::filesystem::path& heldRoot
	);

	[[nodiscard]] static bool FileUnchanged_(
		const domain::ModManifest& held,
		const domain::ManifestFile& file
	);

	[[nodiscard]] static domain::ChunkPlacement Place_(
		const domain::ManifestChunk& chunk,
		const HeldChunkIndex& heldChunks,
		const domain::IHeldChunkCatalogue& catalogue
	);
};
}
