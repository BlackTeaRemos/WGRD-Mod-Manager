#include "manager/trust/ManifestEnvelope.h"

#include "domain/types/distribution/TransportLimits.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

using wgrd::domain::ManifestAuthenticationError;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::Signature;
using wgrd::manager::ManifestEnvelope;

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
