#pragma once

#include "domain/interfaces/content/IManifestDiffer.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"

#include <cstdint>
#include <map>
#include <string>

namespace wgrd::manager {
class ManifestDiffer final : public domain::IManifestDiffer {
public:
	ManifestDiffer();

	~ManifestDiffer() override;

	[[nodiscard]] domain::InstallPlan Diff(
		const domain::ModManifest& held,
		const domain::ModManifest& target
	) const override;

private:
	struct HeldChunk {
		std::string path;
		std::uint64_t offset;
		std::uint32_t length;
	};

	using HeldChunkIndex = std::map<std::string, HeldChunk>;

	[[nodiscard]] static HeldChunkIndex IndexHeldChunks_(const domain::ModManifest& held);

	[[nodiscard]] static bool FileUnchanged_(
		const domain::ModManifest& held,
		const domain::ManifestFile& file
	);
};
}
