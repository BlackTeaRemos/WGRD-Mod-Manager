#include "manager/trust/ManifestAuthenticator.h"

#include "manager/trust/ManifestEnvelope.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

namespace wgrd::manager {

ManifestAuthenticator::ManifestAuthenticator(const domain::IKeyRegistry& registry)
    : _registry(&registry) {
}

ManifestAuthenticator::~ManifestAuthenticator() = default;

std::expected<domain::AuthenticatedManifest, domain::ManifestAuthenticationError>
ManifestAuthenticator::Authenticate(std::span<const std::uint8_t> envelope) const {

    const auto decoded = ManifestEnvelope::Decode(envelope);
    if (!decoded.has_value()) {
        return std::unexpected(decoded.error());
    }

    const auto key = _registry->Find(decoded->fingerprint);
    if (!key.has_value()) {
        return std::unexpected(domain::ManifestAuthenticationError::UnknownPublisher);
    }

    if (key->revoked) {
        return std::unexpected(domain::ManifestAuthenticationError::RevokedPublisher);
    }

    if (!SodiumRuntime::Ready()) {
        return std::unexpected(domain::ManifestAuthenticationError::SignatureInvalid);
    }

    const int verified = crypto_sign_verify_detached(
        decoded->signature.Data(),
        decoded->payload.data(),
        decoded->payload.size(),
        key->publicKey.Data());

    if (verified != 0) {
        return std::unexpected(domain::ManifestAuthenticationError::SignatureInvalid);
    }

    return domain::AuthenticatedManifest{decoded->fingerprint, decoded->payload};
}

}
