#include "manager/publish/ManifestSigner.h"
#include "manager/publish/PassphraseKeyCodec.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/KeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"
#include "manager/trust/ManifestEnvelope.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ManifestAuthenticationError;
using wgrd::domain::ManifestSignerError;
using wgrd::domain::RegisteredKey;
using wgrd::domain::SigningKeyStoreError;
using wgrd::manager::FingerprintDeriver;
using wgrd::manager::KeyRegistry;
using wgrd::manager::ManifestAuthenticator;
using wgrd::manager::ManifestEnvelope;
using wgrd::manager::ManifestSigner;
using wgrd::manager::PassphraseKeyCodec;
using wgrd::manager::SigningKeyStore;

namespace {
constexpr std::string_view PUBLISHER = "tester";
constexpr std::string_view PASSPHRASE = "correct horse battery";

class TemporaryKeyPath {
public:
	explicit TemporaryKeyPath(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label / "publisher.wgrdkey";

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

TEST_CASE("create writes a key and unlocks it") {
	const TemporaryKeyPath keyPath("create");
	SigningKeyStore store;

	REQUIRE_FALSE(store.Unlocked());

	const auto identity = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);

	REQUIRE(identity.has_value());
	REQUIRE(store.Unlocked());
	REQUIRE(std::filesystem::is_regular_file(keyPath.Value()));
	REQUIRE(store.PublisherName() == PUBLISHER);
	REQUIRE(identity->fingerprint == FingerprintDeriver::Derive(identity->publicKey));
}

TEST_CASE("create refuses to overwrite") {
	const TemporaryKeyPath keyPath("overwrite");
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());

	const auto second = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);

	REQUIRE_FALSE(second.has_value());
	REQUIRE(second.error() == SigningKeyStoreError::AlreadyPresent);
}

TEST_CASE("create refuses a short passphrase") {
	const TemporaryKeyPath keyPath("short");
	SigningKeyStore store;

	const auto created = store.Create(keyPath.Value(), PUBLISHER, "short");

	REQUIRE_FALSE(created.has_value());
	REQUIRE(created.error() == SigningKeyStoreError::PassphraseTooShort);
	REQUIRE_FALSE(std::filesystem::exists(keyPath.Value()));
}

TEST_CASE("create refuses an unacceptable publisher name") {
	const TemporaryKeyPath keyPath("badname");
	SigningKeyStore store;

	const auto created = store.Create(keyPath.Value(), "bad name/slash", PASSPHRASE);

	REQUIRE_FALSE(created.has_value());
	REQUIRE(created.error() == SigningKeyStoreError::PublisherRejected);
}

TEST_CASE("unlock recovers the same identity and publisher") {
	const TemporaryKeyPath keyPath("unlock");
	SigningKeyStore store;

	const auto created = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);
	REQUIRE(created.has_value());

	store.Lock();
	REQUIRE_FALSE(store.Unlocked());

	const auto unlocked = store.Unlock(keyPath.Value(), PASSPHRASE);

	REQUIRE(unlocked.has_value());
	REQUIRE(unlocked->publicKey == created->publicKey);
	REQUIRE(unlocked->fingerprint == created->fingerprint);
	REQUIRE(store.PublisherName() == PUBLISHER);
}

TEST_CASE("unlock rejects a wrong passphrase") {
	const TemporaryKeyPath keyPath("wrongpass");
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());
	store.Lock();

	const auto unlocked = store.Unlock(keyPath.Value(), "wrong passphrase");

	REQUIRE_FALSE(unlocked.has_value());
	REQUIRE(unlocked.error() == SigningKeyStoreError::PassphraseRejected);
	REQUIRE_FALSE(store.Unlocked());
}

TEST_CASE("unlock rejects a tampered key file") {
	const TemporaryKeyPath keyPath("tamperkey");
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());
	store.Lock();

	std::vector<char> raw;
	{
		std::ifstream input(keyPath.Value(), std::ios::binary);
		raw.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	}

	raw.back() ^= 0x01;

	{
		std::ofstream output(keyPath.Value(), std::ios::binary | std::ios::trunc);
		output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
	}

	const auto unlocked = store.Unlock(keyPath.Value(), PASSPHRASE);

	REQUIRE_FALSE(unlocked.has_value());
	REQUIRE(unlocked.error() == SigningKeyStoreError::PassphraseRejected);
}

TEST_CASE("unlock rejects a foreign header") {
	const TemporaryKeyPath keyPath("foreign");
	SigningKeyStore store;

	std::error_code failure;
	std::filesystem::create_directories(keyPath.Value().parent_path(), failure);

	{
		std::ofstream output(keyPath.Value(), std::ios::binary | std::ios::trunc);
		const std::string filler(256, 'x');
		output << filler;
	}

	const auto unlocked = store.Unlock(keyPath.Value(), PASSPHRASE);

	REQUIRE_FALSE(unlocked.has_value());
	REQUIRE(unlocked.error() == SigningKeyStoreError::Corrupt);
}

TEST_CASE("unlock reports a missing key file") {
	const TemporaryKeyPath keyPath("missing");
	SigningKeyStore store;

	const auto unlocked = store.Unlock(keyPath.Value(), PASSPHRASE);

	REQUIRE_FALSE(unlocked.has_value());
	REQUIRE(unlocked.error() == SigningKeyStoreError::Absent);
}

TEST_CASE("signing while locked is refused") {
	SigningKeyStore store;

	const auto signature = store.Sign(MakePayload());

	REQUIRE_FALSE(signature.has_value());
	REQUIRE(signature.error() == SigningKeyStoreError::Locked);
}

TEST_CASE("lock clears the publisher name") {
	const TemporaryKeyPath keyPath("locking");
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());

	store.Lock();

	REQUIRE(store.PublisherName().empty());
	REQUIRE_FALSE(store.Identity().has_value());
}

TEST_CASE("sealed key rejects an oversized file") {
	const std::vector<std::uint8_t> oversized(PassphraseKeyCodec::MAXIMUM_SEALED_BYTES + 1, 0);

	const auto opened = PassphraseKeyCodec::Open(oversized, PASSPHRASE);

	REQUIRE_FALSE(opened.has_value());
}

TEST_CASE("sealed manifest authenticates against a registered key") {
	const TemporaryKeyPath keyPath("seal");
	SigningKeyStore store;

	const auto identity = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);
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
	SigningKeyStore store;

	const auto identity = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);
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
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());

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
	SigningKeyStore store;

	const auto identity = store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE);
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
	SigningKeyStore store;

	REQUIRE(store.Create(keyPath.Value(), PUBLISHER, PASSPHRASE).has_value());

	const ManifestSigner signer(store);
	const auto sealed = signer.Seal({});

	REQUIRE_FALSE(sealed.has_value());
	REQUIRE(sealed.error() == ManifestSignerError::PayloadEmpty);
}
