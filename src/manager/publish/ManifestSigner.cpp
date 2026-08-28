#include "manager/publish/ManifestSigner.h"

#include "domain/types/distribution/TransportLimits.h"
#include "manager/trust/ManifestEnvelope.h"

namespace wgrd::manager {

ManifestSigner::ManifestSigner(const domain::ISigningKeyStore& keyStore)
    : _keyStore(&keyStore) {
}

ManifestSigner::~ManifestSigner() = default;

std::expected<std::vector<std::uint8_t>, domain::ManifestSignerError> ManifestSigner::Seal(
    std::span<const std::uint8_t> payload) const {

    if (payload.empty()) {
        return std::unexpected(domain::ManifestSignerError::PayloadEmpty);
    }

    if (payload.size() + ManifestEnvelope::HEADER_BYTES > domain::limits::MANIFEST_ENVELOPE_BYTES) {
        return std::unexpected(domain::ManifestSignerError::PayloadTooLarge);
    }

    const auto identity = _keyStore->Identity();
    if (!identity.has_value()) {
        return std::unexpected(domain::ManifestSignerError::KeyUnavailable);
    }

    const auto signature = _keyStore->Sign(payload);
    if (!signature.has_value()) {
        return std::unexpected(domain::ManifestSignerError::SigningFailed);
    }

    return ManifestEnvelope::Encode(identity->fingerprint, *signature, payload);
}

}
