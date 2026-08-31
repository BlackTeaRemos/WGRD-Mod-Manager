#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/content/IManifestCodec.h"
#include "domain/interfaces/trust/IManifestAuthenticator.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/SignedAnnounce.h"
#include "manager/manifest/ManifestStore.h"

#include <filesystem>
#include <optional>

namespace wgrd::manager {
class FetchedManifestAdopter {
public:
	FetchedManifestAdopter(
		const domain::IContentHasher& hasher,
		const domain::IManifestAuthenticator& authenticator,
		const domain::IManifestCodec& codec,
		const ManifestStore& store
	);

	[[nodiscard]] std::optional<domain::ModManifest> Adopt(
		const domain::SignedAnnounce& announce,
		const std::filesystem::path& stagedManifest
	) const;

private:
	const domain::IContentHasher* _hasher;
	const domain::IManifestAuthenticator* _authenticator;
	const domain::IManifestCodec* _codec;
	const ManifestStore* _store;
};
}
