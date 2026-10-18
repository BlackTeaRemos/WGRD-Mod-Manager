#pragma once

#include "domain/interfaces/services/IInstallService.h"

#include <string_view>

namespace wgrd::gui {
class InstallStartText {
public:
	[[nodiscard]] static constexpr std::string_view Describe(const domain::InstallStartError failure) {
		switch (failure) {
			case domain::InstallStartError::Busy:
				return "Another operation active";
			case domain::InstallStartError::UnknownIdentifier:
				return "Mod no longer available";
			case domain::InstallStartError::ManifestMissing:
				return "Manifest unavailable";
			case domain::InstallStartError::ManifestRejected:
				return "Manifest verification failed";
			case domain::InstallStartError::NothingToDo:
				return "Already current";
			case domain::InstallStartError::NameConflict:
				return "Mod name already used";
			case domain::InstallStartError::FolderOwnershipUnknown:
				return "Folder ownership unknown";
		}
		return "Install could not start";
	}
};
}
