#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/IModRemovalService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "manager/install/InstalledReleaseStore.h"

#include <filesystem>
#include <string>

namespace wgrd::manager {
class ModRemovalService final : public domain::IModRemovalService {
public:
	ModRemovalService(
		std::filesystem::path modsDirectory,
		const InstalledReleaseStore& installed,
		const domain::IAnnounceCatalogue& catalogue,
		domain::IOrderService* order,
		domain::ISeedingService* seeding,
		const domain::IInstallService* install
	);

	~ModRemovalService() override;

	[[nodiscard]] std::expected<void, domain::RemovalError> Remove(
		std::string_view identifier
	) override;

	[[nodiscard]] const std::string& LastMessage() const override;

private:
	[[nodiscard]] std::expected<std::string, domain::RemovalError> ResolveModName_(
		std::string_view identifier
	) const;

	[[nodiscard]] bool WithinModsDirectory_(const std::filesystem::path& target) const;

	void DropFromOrder_(const std::string& modName);

	std::filesystem::path _modsDirectory;
	const InstalledReleaseStore* _installed;
	const domain::IAnnounceCatalogue* _catalogue;
	domain::IOrderService* _order;
	domain::ISeedingService* _seeding;
	const domain::IInstallService* _install;
	std::string _message;
};
}
