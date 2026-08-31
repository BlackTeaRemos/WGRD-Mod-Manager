#include "manager/service/CatalogService.h"

#include "manager/scan/ModFolderScanner.h"
#include "manager/text/ServiceText.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {
CatalogService::CatalogService(
	std::filesystem::path modsDirectory,
	const AnnounceReceiver& receiver,
	const ManifestStore& store,
	const domain::IManifestAuthenticator& authenticator,
	const domain::IManifestCodec& codec,
	domain::IKeyRegistry& registry,
	const InstalledReleaseStore& installed,
	domain::ISeedingService* seeding
)
	: _modsDirectory(std::move(modsDirectory))
	, _receiver(&receiver)
	, _store(&store)
	, _authenticator(&authenticator)
	, _codec(&codec)
	, _registry(&registry)
	, _installed(&installed)
	, _seeding(seeding)
	, _rows()
	, _verified() {
	Refresh();
}

CatalogService::~CatalogService() = default;

domain::CatalogRow CatalogService::Describe_(
	const domain::SignedAnnounce& announce,
	const std::vector<std::string>& installedFolders
) const {
	const bool installed = std::ranges::find(installedFolders, announce.modName
	                       ) != installedFolders.end();

	domain::CatalogRow row{
		announce.Identifier(), announce.modName, announce.publisher.ToHex(), announce.version, 0, 0, 0, installed, false, 0, !_registry->IsUsable(announce.publisher)
	};

	if (installed) {
		if (const auto record = _installed->Find(announce.Identifier())) {
			row.installedVersion = record->version;
		}
	}

	const auto sealed = _store->Load(announce.manifestDigest.ToHex());
	if (!sealed.has_value()) {
		return row;
	}

	const auto authenticated = _authenticator->Authenticate(*sealed);
	if (!authenticated.has_value()) {
		return row;
	}

	const auto manifest = _codec->Decode(authenticated->payload);
	if (!manifest.has_value()) {
		return row;
	}

	if (manifest->ModName() != announce.modName) {
		return row;
	}

	row.totalBytes = manifest->TotalBytes();
	row.chunkCount = manifest->ChunkCount();
	row.fileCount = manifest->Files().size();
	row.manifestHeld = true;

	return row;
}

void CatalogService::Seed_(const domain::SignedAnnounce& announce, domain::CatalogRow& row) {
	if (_seeding == nullptr || !_seeding->Enabled()) {
		return;
	}

	if (row.revoked || !row.installed) {
		const bool stopped = _seeding->StopSeeding(row.identifier);
		(void)stopped;
		return;
	}

	if (row.VersionKnown() && row.installedVersion != row.version) {
		const bool stopped = _seeding->StopSeeding(row.identifier);
		(void)stopped;
		return;
	}

	if (!row.manifestHeld) {
		return;
	}

	const auto sealed = _store->Load(announce.manifestDigest.ToHex());
	if (!sealed.has_value()) {
		return;
	}

	const auto authenticated = _authenticator->Authenticate(*sealed);
	if (!authenticated.has_value()) {
		return;
	}

	const auto manifest = _codec->Decode(authenticated->payload);
	if (!manifest.has_value()) {
		return;
	}

	const auto seeded = _seeding->Announce(
		*manifest,
		_modsDirectory / manifest->ModName(),
		_store->PathFor(announce.manifestDigest.ToHex())
	);

	if (!seeded.has_value()) {
		return;
	}

	if (seeded->infoHash != announce.torrentInfoHash.ToHex()) {
		const bool stopped = _seeding->StopSeeding(row.identifier);
		(void)stopped;
		row.seedFault = text::SEED_INFOHASH_MISMATCH;
	}
}

std::vector<std::string> CatalogService::CollectVerified_() const {
	std::vector<std::string> folders;

	for (const domain::CatalogRow& row : _rows) {
		if (!row.manifestHeld || row.revoked) {
			continue;
		}

		folders.push_back(row.modName);
	}

	return folders;
}

bool CatalogService::Verified(const std::string_view folder) const {
	return std::ranges::find(_verified, folder) != _verified.end();
}

void CatalogService::Refresh() {
	_registry->Reload();

	std::vector<std::string> installedFolders;
	for (const domain::InstalledMod& mod : ModFolderScanner::Scan(_modsDirectory)) {
		installedFolders.push_back(mod.folder.Value());
	}

	_rows.clear();

	for (const domain::SignedAnnounce& announce : _receiver->All()) {
		domain::CatalogRow row = Describe_(announce, installedFolders);

		Seed_(announce, row);

		_rows.push_back(row);
	}

	std::ranges::sort(_rows, [](const domain::CatalogRow& left, const domain::CatalogRow& right) {
		          return left.identifier < right.identifier;
	          }
	);

	_verified = CollectVerified_();
}

const std::vector<domain::CatalogRow>& CatalogService::Rows() const {
	return _rows;
}

std::size_t CatalogService::RegisteredKeys() const {
	return _registry->Count();
}

std::size_t CatalogService::RejectedCount() const {
	return _receiver->RejectedCount();
}
}
