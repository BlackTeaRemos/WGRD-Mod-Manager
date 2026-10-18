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

TEST_CASE("enforces payload path limit") {
	const std::size_t extensionLength = PayloadPathPolicy::PAYLOAD_EXTENSION.size();
	const std::string pathAtLimitMinusOne(
		PayloadPathPolicy::PATH_LIMIT - 1 - extensionLength,
		'a'
	);
	const std::string pathAtLimit(pathAtLimitMinusOne.size() + 1, 'a');

	RequireAccepted(pathAtLimitMinusOne + ".dat", pathAtLimitMinusOne + ".dat");
	RequireAccepted(pathAtLimit + ".dat", pathAtLimit + ".dat");
	RequireRejected(pathAtLimit + "a.dat", PayloadPathError::TooLong);
}

TEST_CASE("rejects embedded nul") {
	std::string path("nested/");
	path.push_back('\0');
	path.append("payload.dat");

	RequireRejected(path, PayloadPathError::ControlCharacter);
}

TEST_CASE("normalisation is idempotent") {
	const auto first = POLICY.Normalise("folder\\nested/MIXED.dat");

	REQUIRE(first.has_value());
	const auto second = POLICY.Normalise(*first);

	REQUIRE(second.has_value());
	REQUIRE(*second == *first);
}

TEST_CASE("rejects reserved device variants") {
	RequireRejected("CON.archive.dat", PayloadPathError::ReservedDeviceName);
	RequireRejected("com1.backup.dat", PayloadPathError::ReservedDeviceName);
	RequireRejected("folder/aux.data.dat", PayloadPathError::ReservedDeviceName);
	RequireAccepted("CONX.dat", "CONX.dat");
}

TEST_CASE("extension check is case insensitive") {
	RequireAccepted("PAYLOAD.DAT", "PAYLOAD.DAT");
	RequireAccepted("Payload.Dat", "Payload.Dat");
}

TEST_CASE("rejects bare extension without stem") {
	RequireRejected(".dat", PayloadPathError::ExtensionNotAllowed);
}

TEST_CASE("Extension root permitted") {
	RequireAccepted("blacktearemos_extensions.dll", "blacktearemos_extensions.dll");
	RequireAccepted("BLACKTEAREMOS_EXTENSIONS.DLL", "blacktearemos_extensions.dll");
}

TEST_CASE("Other DLL paths rejected") {
	RequireRejected("other.dll", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("nested/blacktearemos_extensions.dll", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("blacktearemos_extensions.dll.exe", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("blacktearemos_extensions.dll:payload", PayloadPathError::AlternateStream);
}

TEST_CASE("Extension sidecars rejected") {
	RequireRejected("blacktearemos_extensions.manifest", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("blacktearemos_extensions.sig", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("extensions.required", PayloadPathError::ExtensionNotAllowed);
	RequireRejected("fpv_descriptor_lifetime.log", PayloadPathError::ExtensionNotAllowed);
}
