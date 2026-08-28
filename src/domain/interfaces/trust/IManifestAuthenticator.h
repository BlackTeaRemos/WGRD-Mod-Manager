#pragma once

#include "domain/types/identity/PublisherFingerprint.h"

#include <cstdint>
#include <expected>
#include <span>

namespace wgrd::domain {

enum class ManifestAuthenticationError {
    TooShort,
    TooLarge,
    BadMagic,
    UnsupportedVersion,
    LengthMismatch,
    UnknownPublisher,
    RevokedPublisher,
    SignatureInvalid
};

struct AuthenticatedManifest {
    PublisherFingerprint fingerprint;
    std::span<const std::uint8_t> payload;
};

class IManifestAuthenticator {
public:
    virtual ~IManifestAuthenticator() = 0;

    [[nodiscard]] virtual std::expected<AuthenticatedManifest, ManifestAuthenticationError>
    Authenticate(std::span<const std::uint8_t> envelope) const = 0;
};

inline IManifestAuthenticator::~IManifestAuthenticator() = default;

}
