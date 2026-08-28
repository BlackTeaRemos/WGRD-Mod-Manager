#include "manager/order/OrderFileGateway.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::domain::InstallFolder;
using wgrd::domain::LoadOrder;
using wgrd::domain::OrderEntry;
using wgrd::manager::OrderFileGateway;

namespace {
constexpr std::string_view SHIPPED_FILE =
		"# Wargame Red Dragon mod load order - one mod folder per line.\r\n"
		"# Later entries OVERRIDE earlier ones. '#' comments, blanks ignored.\r\n"
		"test_mod_4\r\n";

InstallFolder MakeFolder(const std::string_view name) {
	const auto folder = InstallFolder::Parse(name);
	REQUIRE(folder.has_value());
	return *folder;
}

LoadOrder MakeOrder(std::vector<OrderEntry> entries) {
	return LoadOrder(std::move(entries));
}
}

TEST_CASE("parses shipped file") {
	const auto order = OrderFileGateway::Parse(SHIPPED_FILE);

	REQUIRE(order.has_value());
	REQUIRE(order->Entries().size() == 1);
	REQUIRE(order->Entries().front().folder.Value() == "test_mod_4");
	REQUIRE(order->Entries().front().enabled);
}

TEST_CASE("round trip preserves bytes") {
	const auto order = OrderFileGateway::Parse(SHIPPED_FILE);
	REQUIRE(order.has_value());

	REQUIRE(OrderFileGateway::Serialize(*order) == SHIPPED_FILE);
}

TEST_CASE("ignores comments and blanks") {
	constexpr std::string_view contents =
			"# header\r\n"
			"\r\n"
			"alpha\r\n"
			"# beta\r\n"
			"\r\n"
			"gamma\r\n";

	const auto order = OrderFileGateway::Parse(contents);

	REQUIRE(order.has_value());
	REQUIRE(order->Entries().size() == 2);
	REQUIRE(order->Entries()[0].folder.Value() == "alpha");
	REQUIRE(order->Entries()[1].folder.Value() == "gamma");
}

TEST_CASE("accepts lf input") {
	constexpr std::string_view contents = "# header\nalpha\ngamma\n";

	const auto order = OrderFileGateway::Parse(contents);

	REQUIRE(order.has_value());
	REQUIRE(order->Entries().size() == 2);
}

TEST_CASE("writes crlf always") {
	const LoadOrder order = MakeOrder({OrderEntry{MakeFolder("alpha"), true}});

	const std::string rendered = OrderFileGateway::Serialize(order);

	REQUIRE(rendered.ends_with("alpha\r\n"));
	REQUIRE(rendered.find("\ralpha") == std::string::npos);
}

TEST_CASE("omits disabled entries") {
	const LoadOrder order = MakeOrder({
			OrderEntry{MakeFolder("alpha"), true}, OrderEntry{MakeFolder("beta"), false}, OrderEntry{MakeFolder("gamma"), true}
		}
	);

	const std::string rendered = OrderFileGateway::Serialize(order);

	REQUIRE(rendered.find("alpha") != std::string::npos);
	REQUIRE(rendered.find("beta") == std::string::npos);
	REQUIRE(rendered.find("gamma") != std::string::npos);
	REQUIRE(order.EnabledCount() == 2);
}

TEST_CASE("empty order keeps header") {
	const std::string rendered = OrderFileGateway::Serialize(LoadOrder());

	REQUIRE(rendered.starts_with("# Wargame Red Dragon"));
	REQUIRE(rendered.ends_with("blanks ignored.\r\n"));
}

TEST_CASE("preserves order of entries") {
	const LoadOrder order = MakeOrder({
			OrderEntry{MakeFolder("zulu"), true}, OrderEntry{MakeFolder("alpha"), true}
		}
	);

	const std::string rendered = OrderFileGateway::Serialize(order);

	REQUIRE(rendered.find("zulu") < rendered.find("alpha"));
}

TEST_CASE("rejects malformed entry") {
	constexpr std::string_view contents = "# header\r\nbad/name\r\n";

	const auto order = OrderFileGateway::Parse(contents);

	REQUIRE_FALSE(order.has_value());
	REQUIRE(order.error() == wgrd::manager::OrderFileError::MalformedEntry);
}

TEST_CASE("missing file reports not found") {
	const auto order = OrderFileGateway::Read("does_not_exist_anywhere.txt");

	REQUIRE_FALSE(order.has_value());
	REQUIRE(order.error() == wgrd::manager::OrderFileError::NotFound);
}
