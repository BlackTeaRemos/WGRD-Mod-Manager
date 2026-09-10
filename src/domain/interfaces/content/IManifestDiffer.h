#pragma once

#include "domain/interfaces/content/IHeldChunkCatalogue.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"

#include <filesystem>

namespace wgrd::domain {
class IManifestDiffer {
public:
	virtual ~IManifestDiffer() = 0;

	[[nodiscard]] virtual InstallPlan Diff(
		const ModManifest& held,
		const std::filesystem::path& heldRoot,
		const ModManifest& target,
		const IHeldChunkCatalogue& catalogue
	) const = 0;
};

inline IManifestDiffer::~IManifestDiffer() = default;
}
