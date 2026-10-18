#include "manager/service/InstallService.h"
#include "manager/identity/ModNameOwnership.h"

namespace wgrd::manager {
std::expected<void, domain::InstallStartError> InstallService::CheckName_(const domain::SignedAnnounce& announce) const {
	if (ModNameOwnership::Conflicts(announce.modName, announce.Identifier(), *_receiver, _installed)) {
		return std::unexpected(domain::InstallStartError::NameConflict);
	}
	std::error_code failure;
	const bool present = std::filesystem::exists(_modsDirectory / announce.modName, failure);
	if (failure || (present && !_installed->Find(announce.Identifier()).has_value())) {
		return std::unexpected(domain::InstallStartError::FolderOwnershipUnknown);
	}
	return {};
}
}
