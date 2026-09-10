#include "manager/install/InstallPlanner.h"

#include "manager/install/EmptyChunkCatalogue.h"
#include "manager/text/ServiceText.h"

#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace wgrd::manager {
InstallPlanner::InstallPlanner(
	std::filesystem::path modsDirectory,
	const domain::IManifestBuilder& manifestBuilder,
	const domain::IManifestDiffer& differ,
	domain::IBaselineIndexService* baseline
)
	: _modsDirectory(std::move(modsDirectory))
	, _manifestBuilder(&manifestBuilder)
	, _differ(&differ)
	, _baseline(baseline) {}

bool InstallPlanner::AwaitBaseline_(
	const domain::ModManifest& target,
	const Abandon& abandon,
	const Report& report
) const {
	if (_baseline == nullptr) {
		return true;
	}

	report(text::INSTALL_INDEXING_BASELINE);

	std::vector<std::string> payloadPaths;
	payloadPaths.reserve(target.Files().size());

	for (const domain::ManifestFile& file : target.Files()) {
		payloadPaths.push_back(file.path);
	}

	const bool indexed = _baseline->AwaitCounterparts(payloadPaths, abandon);
	(void)indexed;

	return !abandon();
}

domain::ModManifest InstallPlanner::DescribeInstalled_(
	const domain::ModManifest& target,
	const std::filesystem::path& modFolder
) const {
	std::error_code failure;
	if (!std::filesystem::is_directory(modFolder, failure)) {
		return domain::ModManifest();
	}

	const auto rebuilt = _manifestBuilder->Build(
		modFolder,
		target.Publisher(),
		target.ModName(),
		0
	);

	if (!rebuilt.has_value()) {
		return domain::ModManifest();
	}

	return *rebuilt;
}

std::optional<domain::InstallPlan> InstallPlanner::Plan(
	const domain::ModManifest& target,
	const Abandon& abandon,
	const Report& report
) const {
	if (!AwaitBaseline_(target, abandon, report)) {
		return std::nullopt;
	}

	report(text::INSTALL_COMPARING);

	const std::filesystem::path modFolder = _modsDirectory / target.ModName();
	const domain::ModManifest held = DescribeInstalled_(target, modFolder);

	const EmptyChunkCatalogue none;
	const domain::IHeldChunkCatalogue& catalogue = _baseline != nullptr ? _baseline->Catalogue() : none;

	return _differ->Diff(held, modFolder, target, catalogue);
}
}
