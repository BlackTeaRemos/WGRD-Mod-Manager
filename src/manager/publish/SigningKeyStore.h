#pragma once

#include "domain/interfaces/trust/IDataProtection.h"
#include "domain/interfaces/trust/ISigningKeyStore.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace wgrd::manager {

class SigningKeyStore final : public domain::ISigningKeyStore {
public:
    SigningKeyStore(std::filesystem::path keyPath, const domain::IDataProtection& protection);

    ~SigningKeyStore() override;

    [[nodiscard]] bool Exists() const override;

    [[nodiscard]] std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> Create() override;

    [[nodiscard]] std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> Identity() const override;

    [[nodiscard]] std::expected<domain::Signature, domain::SigningKeyStoreError> Sign(
        std::span<const std::uint8_t> payload) const override;

private:
    using SecretKey = std::vector<std::uint8_t>;

    [[nodiscard]] std::expected<SecretKey, domain::SigningKeyStoreError> LoadSecretKey_() const;

    [[nodiscard]] std::expected<void, domain::SigningKeyStoreError> StoreSecretKey_(
        std::span<const std::uint8_t> secretKey) const;

    [[nodiscard]] static std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError>
    DeriveIdentity_(std::span<const std::uint8_t> secretKey);

    std::filesystem::path _keyPath;
    const domain::IDataProtection* _protection;
};

}
