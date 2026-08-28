#include "manager/service/CatalogService.h"

#include "manager/scan/ModFolderScanner.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {

CatalogService::CatalogService(
    std::filesystem::path modsDirectory,
    const AnnounceReceiver& receiver,
    const ManifestStore& store,
    const domain::IManifestAuthenticator& authenticator,
    const domain::IManifestCodec& codec,
    const domain::IKeyRegistry& registry,
    domain::ISeedingService* seeding)
    : _modsDirectory(std::move(modsDirectory)),
      _receiver(&receiver),
      _store(&store),
      _authenticator(&authenticator),
      _codec(&codec),
      _registry(&registry),
      _seeding(seeding),
      _rows() {

    Refresh();
}

CatalogService::~CatalogService() = default;

domain::CatalogRow CatalogService::Describe_(
    const domain::SignedAnnounce& announce,
    const std::vector<std::string>& installedFolders) const {

    const bool installed = std::find(
        installedFolders.begin(),
        installedFolders.end(),
        announce.modName) != installedFolders.end();

    domain::CatalogRow row{
        announce.Identifier(),
        announce.modName,
        announce.publisher.ToHex(),
        announce.version,
        0,
        0,
        0,
        installed,
        false
    };

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

    row.totalBytes = manifest->TotalBytes();
    row.chunkCount = manifest->ChunkCount();
    row.fileCount = manifest->Files().size();
    row.manifestHeld = true;

    return row;
}

void CatalogService::Seed_(const domain::SignedAnnounce& announce, const domain::CatalogRow& row) {
    if (_seeding == nullptr || !_seeding->Enabled()) {
        return;
    }

    if (!row.manifestHeld || !row.installed) {
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
        _store->PathFor(announce.manifestDigest.ToHex()));
    (void)seeded;
}

void CatalogService::Refresh() {
    std::vector<std::string> installedFolders;
    for (const domain::InstalledMod& mod : ModFolderScanner::Scan(_modsDirectory)) {
        installedFolders.push_back(mod.folder.Value());
    }

    _rows.clear();

    for (const domain::SignedAnnounce& announce : _receiver->All()) {
        const domain::CatalogRow row = Describe_(announce, installedFolders);

        Seed_(announce, row);

        _rows.push_back(row);
    }

    std::sort(_rows.begin(), _rows.end(), [](const domain::CatalogRow& left, const domain::CatalogRow& right) {
        return left.identifier < right.identifier;
    });
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
