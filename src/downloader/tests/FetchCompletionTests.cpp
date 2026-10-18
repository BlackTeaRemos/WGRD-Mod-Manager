#include "downloader/transfer/FetchState.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>
#include <string>

using wgrd::domain::FetchPhase;
using wgrd::downloader::FetchState;

namespace {
constexpr std::uint64_t WANTED_BYTES = 10000;

void Settle(FetchState& state, const std::uint64_t inFlightBytes, const std::uint64_t pendingWrites) {
	for (int poll = 0; poll <= FetchState::COMPLETE_CONFIRMATIONS; ++poll) {
		state.Update(1, WANTED_BYTES, inFlightBytes, false, pendingWrites == 0, pendingWrites);
	}
}
}

TEST_CASE("fetch completes when wanted bytes land despite redundant payload") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	Settle(state, 4096, 0);

	REQUIRE(state.Snapshot().phase == FetchPhase::Complete);
}

TEST_CASE("fetch waits while writes are outstanding") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	Settle(state, 0, 4);

	REQUIRE(state.Snapshot().phase != FetchPhase::Complete);

	Settle(state, 0, 0);

	REQUIRE(state.Snapshot().phase == FetchPhase::Complete);
}

TEST_CASE("fetch stays busy until wanted bytes are reached") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	for (int poll = 0; poll <= FetchState::COMPLETE_CONFIRMATIONS; ++poll) {
		state.Update(1, WANTED_BYTES - 1, 0, false, true, 0);
	}

	REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
}

TEST_CASE("T20 settled threshold completion") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	for (int confirmation = 0; confirmation < FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		CAPTURE(confirmation);

		state.Update(1, WANTED_BYTES, 0, false, true, 0);

		REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
	}

	state.Update(1, WANTED_BYTES, 0, false, true, 0);

	REQUIRE(state.Snapshot().phase == FetchPhase::Complete);
}

TEST_CASE("T20 unsettled update resets") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	for (int confirmation = 0; confirmation < FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		state.Update(1, WANTED_BYTES, 0, false, true, 0);
	}

	state.Update(1, WANTED_BYTES, 0, true, false, 1);

	REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
	REQUIRE(state.Snapshot().settling);

	for (int confirmation = 0; confirmation < FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		CAPTURE(confirmation);

		state.Update(1, WANTED_BYTES, 0, false, true, 0);

		REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
	}

	state.Update(1, WANTED_BYTES, 0, false, true, 0);

	REQUIRE(state.Snapshot().phase == FetchPhase::Complete);
}

TEST_CASE("T20 incomplete update resets") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));
	state.MarkPrioritised(WANTED_BYTES);

	for (int confirmation = 0; confirmation < FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		state.Update(1, WANTED_BYTES, 0, false, true, 0);
	}

	state.Update(1, WANTED_BYTES - 1, 0, false, true, 0);

	REQUIRE(state.Snapshot().phase != FetchPhase::Complete);

	for (int confirmation = 0; confirmation < FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		CAPTURE(confirmation);

		state.Update(1, WANTED_BYTES, 0, false, true, 0);

		REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
	}

	state.Update(1, WANTED_BYTES, 0, false, true, 0);

	REQUIRE(state.Snapshot().phase == FetchPhase::Complete);
}

TEST_CASE("T20 prioritisation required") {
	FetchState state;

	REQUIRE(state.Reserve("test/mod", "staging", std::set<std::string>{"a.chunk"}));

	for (int confirmation = 0; confirmation <= FetchState::COMPLETE_CONFIRMATIONS; ++confirmation) {
		CAPTURE(confirmation);

		state.Update(1, WANTED_BYTES, 0, true, true, 0);

		REQUIRE(state.Snapshot().phase != FetchPhase::Complete);
	}

	REQUIRE_FALSE(state.Prioritised());
}

TEST_CASE("T20 terminal updates ignored") {
	SECTION("complete") {
		FetchState state;

		REQUIRE(state.Reserve("test/complete", "complete-staging", std::set<std::string>{"a.chunk"}));
		state.MarkPrioritised(WANTED_BYTES);
		Settle(state, 9, 0);

		const wgrd::domain::FetchStatus before = state.Snapshot();

		REQUIRE(before.phase == FetchPhase::Complete);

		state.Update(99, 1, 2, false, false, 3);

		const wgrd::domain::FetchStatus after = state.Snapshot();

		REQUIRE(after.phase == before.phase);
		REQUIRE(after.identifier == before.identifier);
		REQUIRE(after.fetchedBytes == before.fetchedBytes);
		REQUIRE(after.inFlightBytes == before.inFlightBytes);
		REQUIRE(after.wantedBytes == before.wantedBytes);
		REQUIRE(after.peers == before.peers);
		REQUIRE(after.pendingWrites == before.pendingWrites);
		REQUIRE(after.settling == before.settling);
		REQUIRE(after.lastFailure == before.lastFailure);
		REQUIRE(after.stagingFolder == before.stagingFolder);
	}

	SECTION("failed") {
		FetchState state;

		REQUIRE(state.Reserve("test/failed", "failed-staging", std::set<std::string>{"a.chunk"}));
		state.MarkPrioritised(WANTED_BYTES);
		state.Update(4, 5000, 6, false, true, 0);
		state.Fail("storage failed");

		const wgrd::domain::FetchStatus before = state.Snapshot();

		REQUIRE(before.phase == FetchPhase::Failed);

		state.Update(99, 1, 2, false, false, 3);

		const wgrd::domain::FetchStatus after = state.Snapshot();

		REQUIRE(after.phase == before.phase);
		REQUIRE(after.identifier == before.identifier);
		REQUIRE(after.fetchedBytes == before.fetchedBytes);
		REQUIRE(after.inFlightBytes == before.inFlightBytes);
		REQUIRE(after.wantedBytes == before.wantedBytes);
		REQUIRE(after.peers == before.peers);
		REQUIRE(after.pendingWrites == before.pendingWrites);
		REQUIRE(after.settling == before.settling);
		REQUIRE(after.lastFailure == before.lastFailure);
		REQUIRE(after.stagingFolder == before.stagingFolder);
	}
}
