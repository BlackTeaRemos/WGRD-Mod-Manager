#pragma once

#include "domain/types/identity/PublisherIdentity.h"
#include "domain/types/identity/Signature.h"

#include <cstdint>
#include <expected>
#include <span>

namespace wgrd::domain {

enum class SigningKeyStoreError {
    AlreadyPresent,
    Absent,
    Unreadable,
    Unwritable,
    Corrupt,
    ProtectionFailed,
    GenerationFailed
};

class ISigningKeyStore {
public:
    virtual ~ISigningKeyStore() = 0;

    [[nodiscard]] virtual bool Exists() const = 0;

    [[nodiscard]] virtual std::expected<PublisherIdentity, SigningKeyStoreError> Create() = 0;

    [[nodiscard]] virtual std::expected<PublisherIdentity, SigningKeyStoreError> Identity() const = 0;

    [[nodiscard]] virtual std::expected<Signature, SigningKeyStoreError> Sign(
        std::span<const std::uint8_t> payload) const = 0;
};

inline ISigningKeyStore::~ISigningKeyStore() = default;

}
