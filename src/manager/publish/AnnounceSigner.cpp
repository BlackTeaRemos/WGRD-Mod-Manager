#include "manager/publish/AnnounceSigner.h"

#include "manager/announce/AnnounceCodec.h"

namespace wgrd::manager {

AnnounceSigner::AnnounceSigner(
    const domain::ISigningKeyStore& keyStore,
    const domain::IContentHasher& hasher)
    : _keyStore(&keyStore),
      _hasher(&hasher) {
}

std::expected<domain::SignedAnnounce, AnnounceSignError> AnnounceSigner::Announce(
    const domain::ModManifest& manifest,
    std::span<const std::uint8_t> sealedManifest,
    const domain::ChunkDigest& torrentInfoHash) const {

    const auto identity = _keyStore->Identity();
    if (!identity.has_value()) {
        return std::unexpected(AnnounceSignError::KeyUnavailable);
    }

    if (identity->fingerprint != manifest.Publisher()) {
        return std::unexpected(AnnounceSignError::PublisherMismatch);
    }

    std::vector<std::byte> sealedBytes;
    sealedBytes.reserve(sealedManifest.size());
    for (const std::uint8_t value : sealedManifest) {
        sealedBytes.push_back(static_cast<std::byte>(value));
    }

    domain::SignedAnnounce announce{
        identity->fingerprint,
        manifest.ModName(),
        manifest.Version(),
        _hasher->Hash(sealedBytes),
        torrentInfoHash,
        domain::Signature{}
    };

    const std::vector<std::uint8_t> signable = AnnounceCodec::EncodeSignable(announce);

    const auto signature = _keyStore->Sign(signable);
    if (!signature.has_value()) {
        return std::unexpected(AnnounceSignError::SigningFailed);
    }

    announce.signature = *signature;

    return announce;
}

}
