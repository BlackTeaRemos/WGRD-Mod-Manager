#include "manager/service/ModRemovalService.h"

#include "domain/types/order/InstallFolder.h"
#include "manager/text/ServiceText.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace wgrd::manager {
ModRemovalService::ModRemovalService(
	std::filesystem::path modsDirectory,
	const InstalledReleaseStore& installed,
	const domain::IAnnounceCatalogue& catalogue,
	domain::IOrderService* order,
	domain::ISeedingService* seeding,
	const domain::IInstallService* install
)
	: _modsDirectory(std::move(modsDirectory))
	, _installed(&installed)
	, _catalogue(&catalogue)
	, _order(order)
	, _seeding(seeding)
	, _install(install)
	, _message() {}

ModRemovalService::~ModRemovalService() = default;

std::expected<std::string, domain::RemovalError> ModRemovalService::ResolveModName_(
	const std::string_view identifier
) const {
	if (const auto record = _installed->Find(identifier)) {
		return record->modName;
	}

	for (const domain::AnnounceSummary& summary : _catalogue->Summaries()) {
		if (summary.publisher.ToHex() + "/" + summary.modName == identifier) {
			return summary.modName;
		}
	}

	return std::unexpected(domain::RemovalError::UnknownIdentifier);
}

bool ModRemovalService::WithinModsDirectory_(const std::filesystem::path& target) const {
	std::error_code failure;

	const std::filesystem::path root = std::filesystem::weakly_canonical(_modsDirectory, failure);
	if (failure) {
		return false;
	}

	const std::filesystem::path resolved = std::filesystem::weakly_canonical(target, failure);
	if (failure) {
		return false;
	}

	if (resolved == root) {
		return false;
	}

	return resolved.parent_path() == root;
}

void ModRemovalService::DropFromOrder_(const std::string& modName) {
	if (_order == nullptr) {
		return;
	}

	const auto folder = domain::InstallFolder::Parse(modName);
	if (!folder.has_value()) {
		return;
	}

	_order->SetEnabled(*folder, false);
	_order->Refresh();
}

std::expected<void, domain::RemovalError> ModRemovalService::Remove(const std::string_view identifier) {
	if (_install != nullptr && _install->Progress().Busy()) {
		_message = text::REMOVAL_BUSY;
		return std::unexpected(domain::RemovalError::Busy);
	}

	const auto modName = ResolveModName_(identifier);
	if (!modName.has_value()) {
		_message = text::REMOVAL_UNKNOWN;
		return std::unexpected(modName.error());
	}

	const std::filesystem::path folder = _modsDirectory / *modName;

	if (!WithinModsDirectory_(folder)) {
		_message = text::REMOVAL_OUTSIDE;
		return std::unexpected(domain::RemovalError::FolderRejected);
	}

	std::error_code failure;
	if (!std::filesystem::is_directory(folder, failure) || failure) {
		_message = text::REMOVAL_ABSENT;
		return std::unexpected(domain::RemovalError::NotInstalled);
	}

	if (_seeding != nullptr) {
		_seeding->StopSeeding(identifier);
	}

	DropFromOrder_(*modName);

	std::filesystem::remove_all(folder, failure);
	if (failure) {
		_message = text::REMOVAL_LOCKED;
		return std::unexpected(domain::RemovalError::FolderLocked);
	}

	const bool forgotten = _installed->Remove(identifier);
	(void)forgotten;

	_message = std::string(text::REMOVED_PREFIX) + *modName;

	return {};
}

const std::string& ModRemovalService::LastMessage() const {
	return _message;
}
}
