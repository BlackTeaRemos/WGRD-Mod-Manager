#include "manager/payload/PayloadPathPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using wgrd::domain::PayloadPathError;
using wgrd::manager::PayloadPathPolicy;

namespace {
const PayloadPathPolicy POLICY;

void RequireRejected(const std::string_view path, const PayloadPathError expected) {
	const auto result = POLICY.Normalise(path);

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error() == expected);
}

void RequireAccepted(const std::string_view path, std::string_view normalised) {
	const auto result = POLICY.Normalise(path);

	REQUIRE(result.has_value());
	REQUIRE(*result == normalised);
}
}

TEST_CASE("accepts real pack layout") {
	RequireAccepted("130278/131544/ZZ_Win.dat", "130278/131544/ZZ_Win.dat");
	RequireAccepted("Maps/WarGame/PC/_3x3_Angel_v11.dat", "Maps/WarGame/PC/_3x3_Angel_v11.dat");
}

TEST_CASE("accepts metadata only at root") {
	RequireAccepted("mod.json", "mod.json");
	RequireRejected("nested/mod.json", PayloadPathError::MetadataOutOfPlace);
}

TEST_CASE("normalises backslashes") {
	RequireAccepted("48574\\ZZ_1.dat", "48574/ZZ_1.dat");
}

TEST_CASE("rejects other extensions") {
	RequireRejected("payload.exe", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("readme.txt", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("patcher.log", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("noextension", PayloadPathError::ExtensionNotAllowed);
}

TEST_CASE("rejects traversal") {
	RequireRejected("../outside.dat", PayloadPathError::Traversal);
	RequireRejected("a/../../outside.dat", PayloadPathError::Traversal);
	RequireRejected("./here.dat", PayloadPathError::Traversal);
}

TEST_CASE("rejects absolute and drive paths") {
	RequireRejected("/rooted.dat", PayloadPathError::Absolute);
	RequireRejected("C:/windows/system32/evil.dat", PayloadPathError::AlternateStream);
	RequireRejected("\\\\server\\share\\evil.dat", PayloadPathError::Absolute);
}

TEST_CASE("rejects alternate data stream") {
	RequireRejected("payload.dat:evil.exe", PayloadPathError::AlternateStream);
}

TEST_CASE("rejects reserved device names") {
	RequireRejected("CON.dat", PayloadPathError::ReservedDeviceName);
	RequireRejected("nul.dat", PayloadPathError::ReservedDeviceName);
	RequireRejected("COM1.dat", PayloadPathError::ReservedDeviceName);
	RequireRejected("folder/LPT9.dat", PayloadPathError::ReservedDeviceName);
}

TEST_CASE("rejects trailing dot or space") {
	RequireRejected("payload.dat.", PayloadPathError::TrailingDotOrSpace);
	RequireRejected("payload.dat ", PayloadPathError::TrailingDotOrSpace);
	RequireRejected("folder /payload.dat", PayloadPathError::TrailingDotOrSpace);
}

TEST_CASE("rejects control characters") {
	RequireRejected(std::string("bad\x01name.dat"), PayloadPathError::ControlCharacter);
}

TEST_CASE("rejects empty and oversized") {
	RequireRejected("", PayloadPathError::Empty);
	RequireRejected(std::string(PayloadPathPolicy::PATH_LIMIT + 1, 'a'), PayloadPathError::TooLong);
	RequireRejected("folder//payload.dat", PayloadPathError::EmptyComponent);
}

TEST_CASE("extension check is case insensitive") {
	RequireAccepted("PAYLOAD.DAT", "PAYLOAD.DAT");
	RequireAccepted("Payload.Dat", "Payload.Dat");
}

TEST_CASE("rejects bare extension without stem") {
	RequireRejected(".dat", PayloadPathError::ExtensionNotAllowed);
}
