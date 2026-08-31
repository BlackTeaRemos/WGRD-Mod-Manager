#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "domain/types/content/ChunkDestination.h"
#include "domain/types/distribution/InstallPlan.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class FetchPlanner {
public:
	struct Request {
		std::vector<std::string> wantedFiles;
		std::vector<domain::ChunkDestination> destinations;
		bool resumed = false;
	};

	explicit FetchPlanner(const domain::IContentHasher& hasher);

	[[nodiscard]] std::optional<Request> Stage(
		const domain::InstallPlan& plan,
		const std::filesystem::path& modFolder,
		std::string_view stagingSuffix
	) const;

private:
	const domain::IContentHasher* _hasher;
};
}
