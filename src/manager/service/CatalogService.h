#pragma once

#include "domain/interfaces/content/IManifestCodec.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/trust/IKeyRegistry.h"
#include "domain/interfaces/trust/IManifestAuthenticator.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestStore.h"

#include <filesystem>
#include <vector>

namespace wgrd::manager {
class CatalogService final : public domain::ICatalogService {
public:
	CatalogService(
		std::filesystem::path modsDirectory,
		const AnnounceReceiver& receiver,
		const ManifestStore& store,
		const domain::IManifestAuthenticator& authenticator,
		const domain::IManifestCodec& codec,
		domain::IKeyRegistry& registry,
		const InstalledReleaseStore& installed,
		domain::ISeedingService* seeding
	);

	~CatalogService() override;

	void Refresh() override;

	[[nodiscard]] const std::vector<domain::CatalogRow>& Rows() const override;

	[[nodiscard]] std::size_t RegisteredKeys() const override;

	[[nodiscard]] std::size_t RejectedCount() const override;

	[[nodiscard]] bool Verified(std::string_view folder) const override;

private:
	void Seed_(const domain::SignedAnnounce& announce, domain::CatalogRow& row);

	[[nodiscard]] std::vector<std::string> CollectVerified_() const;

	[[nodiscard]] domain::CatalogRow Describe_(
		const domain::SignedAnnounce& announce,
		const std::vector<std::string>& installedFolders
	) const;

	std::filesystem::path _modsDirectory;
	const AnnounceReceiver* _receiver;
	const ManifestStore* _store;
	const domain::IManifestAuthenticator* _authenticator;
	const domain::IManifestCodec* _codec;
	domain::IKeyRegistry* _registry;
	const InstalledReleaseStore* _installed;
	domain::ISeedingService* _seeding;
	std::vector<domain::CatalogRow> _rows;
	std::vector<std::string> _verified;
};
}
