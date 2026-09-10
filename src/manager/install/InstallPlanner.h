#pragma once

#include "domain/interfaces/content/IManifestBuilder.h"
#include "domain/interfaces/content/IManifestDiffer.h"
#include "domain/interfaces/services/IBaselineIndexService.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>

namespace wgrd::manager {
class InstallPlanner {
public:
	using Abandon = std::function<bool()>;
	using Report = std::function<void(std::string_view)>;

	InstallPlanner(
		std::filesystem::path modsDirectory,
		const domain::IManifestBuilder& manifestBuilder,
		const domain::IManifestDiffer& differ,
		domain::IBaselineIndexService* baseline
	);

	[[nodiscard]] std::optional<domain::InstallPlan> Plan(
		const domain::ModManifest& target,
		const Abandon& abandon,
		const Report& report
	) const;

private:
	[[nodiscard]] bool AwaitBaseline_(
		const domain::ModManifest& target,
		const Abandon& abandon,
		const Report& report
	) const;

	[[nodiscard]] domain::ModManifest DescribeInstalled_(
		const domain::ModManifest& target,
		const std::filesystem::path& modFolder
	) const;

	std::filesystem::path _modsDirectory;
	const domain::IManifestBuilder* _manifestBuilder;
	const domain::IManifestDiffer* _differ;
	domain::IBaselineIndexService* _baseline;
};
}
