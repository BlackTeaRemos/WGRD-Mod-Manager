#include "manager/tests/ux/OrderHistoryFixture.h"
#include "manager/order/OrderFileIdentity.h"
#include "manager/hash/Blake3Hasher.h"

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <span>

TEST_CASE("order identity hashes bytes") {
	const wgrd::manager::tests::OrderHistoryFixture fixture;
	const wgrd::manager::Blake3Hasher hasher;
	const auto path = fixture.Installation().orderFile;
	const wgrd::manager::OrderFileIdentity identity(path, hasher);

	SECTION("missing order is unavailable") {
		REQUIRE_FALSE(identity.ReadHash().has_value());
	}
	SECTION("actual contents determine identity") {
		const std::string text = "alpha\r\n\r\nbeta\n";
		std::ofstream output(path, std::ios::binary);
		output << text;
		output.close();
		const auto actual = identity.ReadHash();
		REQUIRE(actual.has_value());
		REQUIRE(*actual == hasher.Hash(std::as_bytes(std::span(text.data(), text.size()))));
	}
	SECTION("oversized order is refused") {
		std::ofstream output(path, std::ios::binary);
		output.close();
		std::filesystem::resize_file(path, 1024u * 1024u + 1);
		const auto actual = identity.ReadHash();
		REQUIRE_FALSE(actual.has_value());
		REQUIRE(actual.error() == wgrd::domain::OrderIdentityError::TooLarge);
	}
}
