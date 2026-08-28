#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/trust/ISigningKeyStore.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/SignedAnnounce.h"

#include <cstdint>
#include <expected>
#include <span>

namespace wgrd::manager {
enum class AnnounceSignError {
	KeyUnavailable, PublisherMismatch, SigningFailed
};

class AnnounceSigner {
public:
	AnnounceSigner(const domain::ISigningKeyStore& keyStore, const domain::IContentHasher& hasher);

	[[nodiscard]] std::expected<domain::SignedAnnounce, AnnounceSignError> Announce(
		const domain::ModManifest& manifest,
		std::span<const std::uint8_t> sealedManifest,
		const domain::ChunkDigest& torrentInfoHash
	) const;

private:
	const domain::ISigningKeyStore* _keyStore;
	const domain::IContentHasher* _hasher;
};
}
