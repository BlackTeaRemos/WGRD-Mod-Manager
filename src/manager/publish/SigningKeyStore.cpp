#include "manager/publish/SigningKeyStore.h"

#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace wgrd::manager {

namespace {

constexpr std::size_t SECRET_KEY_BYTES = crypto_sign_SECRETKEYBYTES;
constexpr std::size_t PUBLIC_KEY_BYTES = crypto_sign_PUBLICKEYBYTES;

}

SigningKeyStore::SigningKeyStore(std::filesystem::path keyPath, const domain::IDataProtection& protection)
    : _keyPath(std::move(keyPath)),
      _protection(&protection) {
}

SigningKeyStore::~SigningKeyStore() = default;

bool SigningKeyStore::Exists() const {
    std::error_code failure;
    return std::filesystem::is_regular_file(_keyPath, failure);
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::Create() {
    if (Exists()) {
        return std::unexpected(domain::SigningKeyStoreError::AlreadyPresent);
    }

    if (!SodiumRuntime::Ready()) {
        return std::unexpected(domain::SigningKeyStoreError::GenerationFailed);
    }

    std::array<std::uint8_t, PUBLIC_KEY_BYTES> publicKeyBytes{};
    std::array<std::uint8_t, SECRET_KEY_BYTES> secretKeyBytes{};

    if (crypto_sign_keypair(publicKeyBytes.data(), secretKeyBytes.data()) != 0) {
        sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
        return std::unexpected(domain::SigningKeyStoreError::GenerationFailed);
    }

    const auto stored = StoreSecretKey_(secretKeyBytes);
    if (!stored.has_value()) {
        sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
        return std::unexpected(stored.error());
    }

    const auto identity = DeriveIdentity_(secretKeyBytes);
    sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());

    return identity;
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::Identity() const {
    auto secretKey = LoadSecretKey_();
    if (!secretKey.has_value()) {
        return std::unexpected(secretKey.error());
    }

    const auto identity = DeriveIdentity_(*secretKey);
    sodium_memzero(secretKey->data(), secretKey->size());

    return identity;
}

std::expected<domain::Signature, domain::SigningKeyStoreError> SigningKeyStore::Sign(
    std::span<const std::uint8_t> payload) const {

    if (!SodiumRuntime::Ready()) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    auto secretKey = LoadSecretKey_();
    if (!secretKey.has_value()) {
        return std::unexpected(secretKey.error());
    }

    std::array<std::uint8_t, crypto_sign_BYTES> signatureBytes{};
    const int signingResult = crypto_sign_detached(
        signatureBytes.data(),
        nullptr,
        payload.data(),
        payload.size(),
        secretKey->data());

    sodium_memzero(secretKey->data(), secretKey->size());

    if (signingResult != 0) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    const auto signature = domain::Signature::FromBytes(signatureBytes);
    if (!signature.has_value()) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    return *signature;
}

std::expected<SigningKeyStore::SecretKey, domain::SigningKeyStoreError> SigningKeyStore::LoadSecretKey_() const {
    if (!Exists()) {
        return std::unexpected(domain::SigningKeyStoreError::Absent);
    }

    std::ifstream input(_keyPath, std::ios::binary);
    if (!input) {
        return std::unexpected(domain::SigningKeyStoreError::Unreadable);
    }

    const std::istreambuf_iterator<char> first(input);
    const std::istreambuf_iterator<char> last;
    const std::string raw(first, last);

    std::vector<std::uint8_t> ciphertext;
    ciphertext.reserve(raw.size());
    for (const char character : raw) {
        ciphertext.push_back(static_cast<std::uint8_t>(character));
    }

    if (ciphertext.empty()) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    auto plaintext = _protection->Unprotect(ciphertext);
    if (!plaintext.has_value()) {
        return std::unexpected(domain::SigningKeyStoreError::ProtectionFailed);
    }

    if (plaintext->size() != SECRET_KEY_BYTES) {
        sodium_memzero(plaintext->data(), plaintext->size());
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    return *plaintext;
}

std::expected<void, domain::SigningKeyStoreError> SigningKeyStore::StoreSecretKey_(
    std::span<const std::uint8_t> secretKey) const {

    auto ciphertext = _protection->Protect(secretKey);
    if (!ciphertext.has_value()) {
        return std::unexpected(domain::SigningKeyStoreError::ProtectionFailed);
    }

    std::error_code failure;
    std::filesystem::create_directories(_keyPath.parent_path(), failure);

    std::ofstream output(_keyPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected(domain::SigningKeyStoreError::Unwritable);
    }

    output.write(
        reinterpret_cast<const char*>(ciphertext->data()),
        static_cast<std::streamsize>(ciphertext->size()));

    if (!output) {
        return std::unexpected(domain::SigningKeyStoreError::Unwritable);
    }

    return {};
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::DeriveIdentity_(
    std::span<const std::uint8_t> secretKey) {

    if (secretKey.size() != SECRET_KEY_BYTES) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    std::array<std::uint8_t, PUBLIC_KEY_BYTES> publicKeyBytes{};
    if (crypto_sign_ed25519_sk_to_pk(publicKeyBytes.data(), secretKey.data()) != 0) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    const auto publicKey = domain::PublicKey::FromBytes(publicKeyBytes);
    if (!publicKey.has_value()) {
        return std::unexpected(domain::SigningKeyStoreError::Corrupt);
    }

    return domain::PublisherIdentity{FingerprintDeriver::Derive(*publicKey), *publicKey};
}

}
