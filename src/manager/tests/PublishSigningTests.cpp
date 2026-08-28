#include "manager/publish/ManifestSigner.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/KeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"
#include "manager/trust/ManifestEnvelope.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::DataProtectionError;
using wgrd::domain::IDataProtection;
using wgrd::domain::ManifestAuthenticationError;
using wgrd::domain::ManifestSignerError;
using wgrd::domain::RegisteredKey;
using wgrd::domain::SigningKeyStoreError;
using wgrd::manager::FingerprintDeriver;
using wgrd::manager::KeyRegistry;
using wgrd::manager::ManifestAuthenticator;
using wgrd::manager::ManifestEnvelope;
using wgrd::manager::ManifestSigner;
using wgrd::manager::SigningKeyStore;

namespace {

class TransparentProtection final : public IDataProtection {
public:
    TransparentProtection() = default;

    ~TransparentProtection() override = default;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, DataProtectionError> Protect(
        std::span<const std::uint8_t> plaintext) const override {
        return std::vector<std::uint8_t>(plaintext.begin(), plaintext.end());
    }

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, DataProtectionError> Unprotect(
        std::span<const std::uint8_t> ciphertext) const override {
        return std::vector<std::uint8_t>(ciphertext.begin(), ciphertext.end());
    }
};

class TemporaryKeyPath {
public:
    explicit TemporaryKeyPath(std::string_view label) {
        _path = std::filesystem::temp_directory_path() / "wgrd-tests" / label / "publisher.key";

        std::error_code failure;
        std::filesystem::remove_all(_path.parent_path(), failure);
    }

    ~TemporaryKeyPath() {
        std::error_code failure;
        std::filesystem::remove_all(_path.parent_path(), failure);
    }

    [[nodiscard]] const std::filesystem::path& Value() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

std::vector<std::uint8_t> MakePayload() {
    const std::string text = R"({"name":"angel_maps","version":"1.2.0"})";
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

}

TEST_CASE("create writes a key and derives identity") {
    const TemporaryKeyPath keyPath("create");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    REQUIRE_FALSE(store.Exists());

    const auto identity = store.Create();

    REQUIRE(identity.has_value());
    REQUIRE(store.Exists());
    REQUIRE(identity->fingerprint == FingerprintDeriver::Derive(identity->publicKey));
}

TEST_CASE("create refuses to overwrite") {
    const TemporaryKeyPath keyPath("overwrite");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    REQUIRE(store.Create().has_value());

    const auto second = store.Create();

    REQUIRE_FALSE(second.has_value());
    REQUIRE(second.error() == SigningKeyStoreError::AlreadyPresent);
}

TEST_CASE("identity reloads the same public key") {
    const TemporaryKeyPath keyPath("reload");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    const auto created = store.Create();
    REQUIRE(created.has_value());

    const auto reloaded = store.Identity();

    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->publicKey == created->publicKey);
    REQUIRE(reloaded->fingerprint == created->fingerprint);
}

TEST_CASE("signing without a key reports absent") {
    const TemporaryKeyPath keyPath("absent");
    const TransparentProtection protection;
    const SigningKeyStore store(keyPath.Value(), protection);

    const auto signature = store.Sign(MakePayload());

    REQUIRE_FALSE(signature.has_value());
    REQUIRE(signature.error() == SigningKeyStoreError::Absent);
}

TEST_CASE("sealed manifest authenticates against a registered key") {
    const TemporaryKeyPath keyPath("seal");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    const auto identity = store.Create();
    REQUIRE(identity.has_value());

    const ManifestSigner signer(store);
    const auto sealed = signer.Seal(MakePayload());
    REQUIRE(sealed.has_value());

    const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", false}});
    const ManifestAuthenticator authenticator(registry);

    const auto authenticated = authenticator.Authenticate(*sealed);

    REQUIRE(authenticated.has_value());
    REQUIRE(authenticated->fingerprint == identity->fingerprint);

    const auto payload = MakePayload();
    REQUIRE(authenticated->payload.size() == payload.size());
    REQUIRE(std::equal(payload.begin(), payload.end(), authenticated->payload.begin()));
}

TEST_CASE("tampered payload fails verification") {
    const TemporaryKeyPath keyPath("tamper");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    const auto identity = store.Create();
    REQUIRE(identity.has_value());

    const ManifestSigner signer(store);
    auto sealed = signer.Seal(MakePayload());
    REQUIRE(sealed.has_value());

    (*sealed)[ManifestEnvelope::HEADER_BYTES] ^= 0x01;

    const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", false}});
    const ManifestAuthenticator authenticator(registry);

    const auto authenticated = authenticator.Authenticate(*sealed);

    REQUIRE_FALSE(authenticated.has_value());
    REQUIRE(authenticated.error() == ManifestAuthenticationError::SignatureInvalid);
}

TEST_CASE("unregistered publisher is rejected") {
    const TemporaryKeyPath keyPath("unregistered");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    REQUIRE(store.Create().has_value());

    const ManifestSigner signer(store);
    const auto sealed = signer.Seal(MakePayload());
    REQUIRE(sealed.has_value());

    const KeyRegistry registry({});
    const ManifestAuthenticator authenticator(registry);

    const auto authenticated = authenticator.Authenticate(*sealed);

    REQUIRE_FALSE(authenticated.has_value());
    REQUIRE(authenticated.error() == ManifestAuthenticationError::UnknownPublisher);
}

TEST_CASE("revoked publisher is rejected") {
    const TemporaryKeyPath keyPath("revoked");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    const auto identity = store.Create();
    REQUIRE(identity.has_value());

    const ManifestSigner signer(store);
    const auto sealed = signer.Seal(MakePayload());
    REQUIRE(sealed.has_value());

    const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", true}});
    const ManifestAuthenticator authenticator(registry);

    const auto authenticated = authenticator.Authenticate(*sealed);

    REQUIRE_FALSE(authenticated.has_value());
    REQUIRE(authenticated.error() == ManifestAuthenticationError::RevokedPublisher);
}

TEST_CASE("empty payload is refused") {
    const TemporaryKeyPath keyPath("empty");
    const TransparentProtection protection;
    SigningKeyStore store(keyPath.Value(), protection);

    REQUIRE(store.Create().has_value());

    const ManifestSigner signer(store);
    const auto sealed = signer.Seal({});

    REQUIRE_FALSE(sealed.has_value());
    REQUIRE(sealed.error() == ManifestSignerError::PayloadEmpty);
}
