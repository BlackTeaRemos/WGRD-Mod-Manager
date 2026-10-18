#include "manager/trust/ManifestEnvelope.h"
#include "manager/publish/ManifestSigner.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/KeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/types/distribution/TransportLimits.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <span>
#include <system_error>
#include <vector>

using wgrd::domain::ManifestAuthenticationError;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::Signature;
using wgrd::domain::RegisteredKey;
using wgrd::manager::ManifestEnvelope;
using wgrd::manager::ManifestAuthenticator;
using wgrd::manager::ManifestSigner;
using wgrd::manager::SigningKeyStore;
using wgrd::manager::KeyRegistry;

namespace {
PublisherFingerprint MakeFingerprint() {
	const auto fingerprint = PublisherFingerprint::FromHex("0123456789abcdef");
	REQUIRE(fingerprint.has_value());
	return *fingerprint;
}

Signature MakeSignature() {
	const std::string hex(Signature::HEX_LENGTH, 'a');
	const auto signature = Signature::FromHex(hex);
	REQUIRE(signature.has_value());
	return *signature;
}

std::vector<std::uint8_t> MakePayload(const std::size_t length) {
	std::vector<std::uint8_t> payload(length);
	for (std::size_t position = 0; position < length; ++position) {
		payload[position] = static_cast<std::uint8_t>(position & 0xFF);
	}
	return payload;
}

class TemporaryKeyPath {
public:
	explicit TemporaryKeyPath(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-envelope-tests" / label / "publisher.wgrdkey";
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

struct SealedFixture {
	std::unique_ptr<SigningKeyStore> store;
	std::vector<std::uint8_t> envelope;
	wgrd::domain::PublisherIdentity identity;
};

SealedFixture MakeSealedFixture(const std::string_view label) {
	const TemporaryKeyPath keyPath(label);
	SealedFixture fixture;
	fixture.store = std::make_unique<SigningKeyStore>();
	const auto identity = fixture.store->Create(keyPath.Value(), "tester", "correct horse battery");
	REQUIRE(identity.has_value());
	fixture.identity = *identity;
	const ManifestSigner signer(*fixture.store);
	const std::vector<std::uint8_t> payload = MakePayload(32);
	const auto sealed = signer.Seal(payload);
	REQUIRE(sealed.has_value());
	fixture.envelope = *sealed;
	return fixture;
}
}

TEST_CASE("header occupies eighty eight bytes") {
	const auto envelope = ManifestEnvelope::Encode(MakeFingerprint(), MakeSignature(), MakePayload(10));

	REQUIRE(envelope.size() == ManifestEnvelope::HEADER_BYTES + 10);
}

TEST_CASE("round trip preserves every field") {
	const PublisherFingerprint fingerprint = MakeFingerprint();
	const Signature signature = MakeSignature();
	const std::vector<std::uint8_t> payload = MakePayload(4096);

	const auto envelope = ManifestEnvelope::Encode(fingerprint, signature, payload);
	const auto decoded = ManifestEnvelope::Decode(envelope);

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->fingerprint == fingerprint);
	REQUIRE(decoded->signature == signature);
	REQUIRE(decoded->payload.size() == payload.size());
	REQUIRE(std::equal(payload.begin(), payload.end(), decoded->payload.begin()));
}

TEST_CASE("rejects short buffer") {
	std::vector<std::uint8_t> truncated(ManifestEnvelope::HEADER_BYTES - 1, 0);

	const auto decoded = ManifestEnvelope::Decode(truncated);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == ManifestAuthenticationError::TooShort);
}

TEST_CASE("rejects foreign magic") {
	auto envelope = ManifestEnvelope::Encode(MakeFingerprint(), MakeSignature(), MakePayload(8));
	envelope[0] = static_cast<std::uint8_t>('X');

	const auto decoded = ManifestEnvelope::Decode(envelope);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == ManifestAuthenticationError::BadMagic);
}

TEST_CASE("rejects unknown version") {
	auto envelope = ManifestEnvelope::Encode(MakeFingerprint(), MakeSignature(), MakePayload(8));
	envelope[ManifestEnvelope::VERSION_OFFSET] = 9;

	const auto decoded = ManifestEnvelope::Decode(envelope);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == ManifestAuthenticationError::UnsupportedVersion);
}

TEST_CASE("rejects declared length mismatch") {
	auto envelope = ManifestEnvelope::Encode(MakeFingerprint(), MakeSignature(), MakePayload(8));
	envelope[ManifestEnvelope::PAYLOAD_SIZE_OFFSET] = 99;

	const auto decoded = ManifestEnvelope::Decode(envelope);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == ManifestAuthenticationError::LengthMismatch);
}

TEST_CASE("rejects oversized buffer before reading") {
	const std::vector<std::uint8_t> oversized(wgrd::domain::limits::MANIFEST_ENVELOPE_BYTES + 1, 0);

	const auto decoded = ManifestEnvelope::Decode(oversized);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == ManifestAuthenticationError::TooLarge);
}

TEST_CASE("T07 signature mutation rejected") {
	SealedFixture fixture = MakeSealedFixture("signature");
	fixture.envelope[ManifestEnvelope::SIGNATURE_OFFSET] ^= 0x01;
	const KeyRegistry registry({RegisteredKey{fixture.identity.fingerprint, fixture.identity.publicKey, "tester", false}});
	const ManifestAuthenticator authenticator(registry);

	const auto authenticated = authenticator.Authenticate(fixture.envelope);

	REQUIRE_FALSE(authenticated.has_value());
	REQUIRE(authenticated.error() == ManifestAuthenticationError::SignatureInvalid);
}

TEST_CASE("T07 publisher mutation rejected") {
	SealedFixture fixture = MakeSealedFixture("publisher");
	fixture.envelope[ManifestEnvelope::FINGERPRINT_OFFSET] ^= 0x01;
	const KeyRegistry registry({RegisteredKey{fixture.identity.fingerprint, fixture.identity.publicKey, "tester", false}});
	const ManifestAuthenticator authenticator(registry);

	const auto authenticated = authenticator.Authenticate(fixture.envelope);

	REQUIRE_FALSE(authenticated.has_value());
	REQUIRE(authenticated.error() == ManifestAuthenticationError::UnknownPublisher);
}

TEST_CASE("T07 truncated envelope rejected") {
	SealedFixture fixture = MakeSealedFixture("truncate");
	const std::vector<std::size_t> lengths = {
		0,
		ManifestEnvelope::MAGIC_OFFSET + 1,
		ManifestEnvelope::SIGNATURE_OFFSET,
		ManifestEnvelope::HEADER_BYTES - 1,
		ManifestEnvelope::HEADER_BYTES
	};
	const KeyRegistry registry({RegisteredKey{fixture.identity.fingerprint, fixture.identity.publicKey, "tester", false}});
	const ManifestAuthenticator authenticator(registry);

	for (const std::size_t length : lengths) {
		const std::span<const std::uint8_t> truncated(fixture.envelope.data(), length);
		const auto authenticated = authenticator.Authenticate(truncated);
		REQUIRE_FALSE(authenticated.has_value());
		REQUIRE(authenticated.error() == (length == ManifestEnvelope::HEADER_BYTES
			? ManifestAuthenticationError::LengthMismatch
			: ManifestAuthenticationError::TooShort));
	}
}

TEST_CASE("T07 inconsistent length rejected") {
	SealedFixture fixture = MakeSealedFixture("length");
	fixture.envelope[ManifestEnvelope::PAYLOAD_SIZE_OFFSET] ^= 0x01;
	const KeyRegistry registry({RegisteredKey{fixture.identity.fingerprint, fixture.identity.publicKey, "tester", false}});
	const ManifestAuthenticator authenticator(registry);

	const auto authenticated = authenticator.Authenticate(fixture.envelope);

	REQUIRE_FALSE(authenticated.has_value());
	REQUIRE(authenticated.error() == ManifestAuthenticationError::LengthMismatch);
}
