#include "downloader/transfer/FetchState.h"

#include <catch2/catch_test_macros.hpp>

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
