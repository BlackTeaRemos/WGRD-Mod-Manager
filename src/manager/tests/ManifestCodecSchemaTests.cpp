#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using wgrd::domain::ManifestDecodeError;
using wgrd::manager::ManifestCodec;
using wgrd::manager::PayloadPathPolicy;

constexpr std::string_view VALID = R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat","size":4,"chunks":[{"digest":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","length":4}]}]})";

std::vector<std::uint8_t> Bytes(const std::string_view text) {
	return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::string Replace(const std::string_view source, const std::string_view from, const std::string_view to) {
	std::string result(source);
	const std::size_t position = result.find(from);
	REQUIRE(position != std::string::npos);
	result.replace(position, from.size(), to);
	return result;
}

const ManifestCodec& Codec() {
	static const PayloadPathPolicy policy;
	static const ManifestCodec codec(policy);
	return codec;
}
}

TEST_CASE("T04 valid schema accepted") {
	const auto decoded = Codec().Decode(Bytes(VALID));

	REQUIRE(decoded.has_value());
}

TEST_CASE("T04 required fields rejected") {
	const std::vector<std::string> mutations = {
		R"({"mod":"demo","version":1,"files":[]})",
		R"({"publisher":"0011223344556677","version":1,"files":[]})",
		R"({"publisher":"0011223344556677","mod":"demo","files":[]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{}]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat"}]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat","size":4}]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat","size":4,"chunks":[{}]}]})"
	};

	for (const std::string& mutation : mutations) {
		const auto decoded = Codec().Decode(Bytes(mutation));
		REQUIRE_FALSE(decoded.has_value());
		REQUIRE(decoded.error() == ManifestDecodeError::FieldMissing);
	}
}

TEST_CASE("T04 wrong field types rejected") {
	const std::vector<std::string> mutations = {
		Replace(VALID, R"("publisher":"0011223344556677")", R"("publisher":null)"),
		Replace(VALID, R"("mod":"demo")", R"("mod":false)"),
		Replace(VALID, R"("version":1)", R"("version":-1)"),
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":null})"
	};

	for (const std::string& mutation : mutations) {
		const auto decoded = Codec().Decode(Bytes(mutation));
		REQUIRE_FALSE(decoded.has_value());
		REQUIRE(decoded.error() == ManifestDecodeError::FieldWrongType);
	}
}

TEST_CASE("T04 non object entries rejected") {
	const std::vector<std::string> mutations = {
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[null]})",
		R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[{"path":"pack.dat","size":4,"chunks":[[]]}]})"
	};

	for (const std::string& mutation : mutations) {
		const auto decoded = Codec().Decode(Bytes(mutation));
		REQUIRE_FALSE(decoded.has_value());
		REQUIRE(decoded.error() == ManifestDecodeError::FieldWrongType);
	}
}

TEST_CASE("T04 invalid values rejected") {
	const std::vector<std::pair<std::string, ManifestDecodeError>> mutations = {
		{Replace(VALID, R"("publisher":"0011223344556677")", R"("publisher":"bad")"), ManifestDecodeError::PublisherRejected},
		{Replace(VALID, std::string(64, 'a'), std::string(63, 'a') + "z"), ManifestDecodeError::ChunkLayoutInvalid},
		{R"({"publisher":"0011223344556677","mod":"demo","version":1,"files":[]})", ManifestDecodeError::NoFiles}
	};

	for (const auto& [mutation, expected] : mutations) {
		const auto decoded = Codec().Decode(Bytes(mutation));
		REQUIRE_FALSE(decoded.has_value());
		REQUIRE(decoded.error() == expected);
	}
}
