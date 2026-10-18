#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"

#include "domain/types/distribution/TransportLimits.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
using wgrd::domain::ManifestDecodeError;
using wgrd::manager::ManifestCodec;
using wgrd::manager::PayloadPathPolicy;

std::string Manifest(const std::uint64_t size, const std::uint64_t length, const std::size_t count = 1) {
	std::string result = R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat","size":)";
	result += std::to_string(size) + R"(,"chunks":[)";
	for (std::size_t position = 0; position < count; ++position) {
		if (position != 0) {
			result += ',';
		}
		result += R"({"digest":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","length":)";
		result += std::to_string(length) + "}";
	}
	result += "]}]";
	return result + '}';
}

std::vector<std::uint8_t> Bytes(const std::string& text) {
	return std::vector<std::uint8_t>(text.begin(), text.end());
}

const ManifestCodec& Codec() {
	static const PayloadPathPolicy policy;
	static const ManifestCodec codec(policy);
	return codec;
}
}

TEST_CASE("T05 chunk length boundaries") {
	const auto accepted = Codec().Decode(Bytes(Manifest(wgrd::domain::limits::MANIFEST_CHUNK_LENGTH_BYTES, wgrd::domain::limits::MANIFEST_CHUNK_LENGTH_BYTES)));
	const auto zero = Codec().Decode(Bytes(Manifest(0, 0)));
	const auto oversized = Codec().Decode(Bytes(Manifest(wgrd::domain::limits::MANIFEST_CHUNK_LENGTH_BYTES + 1, wgrd::domain::limits::MANIFEST_CHUNK_LENGTH_BYTES + 1)));

	REQUIRE(accepted.has_value());
	REQUIRE_FALSE(zero.has_value());
	REQUIRE(zero.error() == ManifestDecodeError::ChunkLayoutInvalid);
	REQUIRE_FALSE(oversized.has_value());
	REQUIRE(oversized.error() == ManifestDecodeError::ChunkLayoutInvalid);
}

TEST_CASE("T05 file size boundary") {
	const auto accepted = Codec().Decode(Bytes(Manifest(wgrd::domain::limits::MANIFEST_FILE_BYTES, wgrd::domain::limits::MANIFEST_CHUNK_LENGTH_BYTES, 4096)));
	const auto oversized = Codec().Decode(Bytes(Manifest(wgrd::domain::limits::MANIFEST_FILE_BYTES + 1, 1, 1)));

	REQUIRE(accepted.has_value());
	REQUIRE_FALSE(oversized.has_value());
	REQUIRE(oversized.error() == ManifestDecodeError::ChunkLayoutInvalid);
}

TEST_CASE("T05 aggregate chunk limit") {
	const auto accepted = Codec().Decode(Bytes(Manifest(16384, 1, wgrd::domain::limits::MANIFEST_CHUNK_COUNT)));
	const auto oversized = Codec().Decode(Bytes(Manifest(16385, 1, wgrd::domain::limits::MANIFEST_CHUNK_COUNT + 1)));

	REQUIRE(accepted.has_value());
	REQUIRE_FALSE(oversized.has_value());
	REQUIRE(oversized.error() == ManifestDecodeError::TooManyChunks);
}

TEST_CASE("T05 numeric parser rejects invalid unsigned values") {
	const std::vector<std::string> inputs = {
		R"({"publisher":"0011223344556677","mod":"demo","version":-1,"files":[]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1.5,"files":[]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":18446744073709551616,"files":[]})"
	};

	for (const std::string& input : inputs) {
		const auto decoded = Codec().Decode(Bytes(input));
		REQUIRE_FALSE(decoded.has_value());
		REQUIRE(decoded.error() == ManifestDecodeError::FieldWrongType);
	}
}
