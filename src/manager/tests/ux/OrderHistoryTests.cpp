#include "manager/tests/ux/OrderHistoryFixture.h"

#include "manager/order/OrderFileGateway.h"
#include "manager/service/OrderService.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("order edits undo safely") {
	using namespace wgrd;
	const manager::tests::OrderHistoryFixture fixture;
	manager::OrderService order(fixture.Installation());
	const auto alpha = domain::InstallFolder::Parse("alpha");
	const auto beta = domain::InstallFolder::Parse("beta");
	REQUIRE(alpha.has_value());
	REQUIRE(beta.has_value());
	REQUIRE(order.Current().entries.size() == 2);

	SECTION("toggle and move undo") {
		order.SetEnabled(*alpha, true);
		order.Move(0, 1);
		REQUIRE(order.Current().entries[0].folder == *beta);
		REQUIRE(order.Undo());
		REQUIRE(order.Current().entries[0].folder == *alpha);
		REQUIRE(order.Current().entries[0].enabled);
		REQUIRE(order.Undo());
		REQUIRE_FALSE(order.Current().entries[0].enabled);
		REQUIRE_FALSE(order.CanUndo());
		const auto stored = manager::OrderFileGateway::Read(fixture.Installation().orderFile);
		REQUIRE(stored.has_value());
		REQUIRE(stored->EnabledCount() == 0);
	}

	SECTION("failed write preserves state") {
		std::filesystem::create_directory(fixture.Installation().orderFile);
		order.SetEnabled(*alpha, true);
		REQUIRE_FALSE(order.Current().entries[0].enabled);
		REQUIRE_FALSE(order.Current().writable);
		REQUIRE_FALSE(order.CanUndo());
		REQUIRE(order.LastMessage() == "Load order write failed");
	}

	SECTION("failed undo remains retryable") {
		order.SetEnabled(*alpha, true);
		const auto path = fixture.Installation().orderFile;
		const auto backup = fixture.Root() / "saved_order.txt";
		std::filesystem::rename(path, backup);
		std::filesystem::create_directory(path);
		REQUIRE_FALSE(order.Undo());
		REQUIRE(order.CanUndo());
		REQUIRE(order.Current().entries[0].enabled);
		std::filesystem::remove(path);
		std::filesystem::rename(backup, path);
		REQUIRE(order.Undo());
		REQUIRE_FALSE(order.Current().entries[0].enabled);
	}

	SECTION("profile change clears history") {
		order.SetEnabled(*alpha, true);
		REQUIRE(order.Apply(domain::LoadOrder({domain::OrderEntry{*beta, true}})));
		REQUIRE_FALSE(order.CanUndo());
		REQUIRE(order.Current().entries[0].folder == *beta);
	}

	SECTION("external edits clear history") {
		order.SetEnabled(*alpha, true);
		REQUIRE(manager::OrderFileGateway::Write(fixture.Installation().orderFile,
			domain::LoadOrder({domain::OrderEntry{*beta, true}})).has_value());
		order.Refresh();
		REQUIRE_FALSE(order.CanUndo());
		REQUIRE(order.Current().entries[0].folder == *beta);
	}
}
