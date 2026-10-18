#include "downloader/announce/OutstandingWantTracker.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::downloader::OutstandingWantTracker;

TEST_CASE("T23 duplicate tracking deduplicates") {
	OutstandingWantTracker tracker;
	const auto now = OutstandingWantTracker::Clock::now();

	REQUIRE(tracker.Track("publisher/mod/7", now));
	REQUIRE(tracker.Track("publisher/mod/7", now));
	REQUIRE(tracker.Count() == 1);
	REQUIRE(tracker.Redeem("publisher/mod/7"));
	REQUIRE(tracker.Count() == 0);
}

TEST_CASE("T23 mismatched redemption preserves") {
	OutstandingWantTracker tracker;
	const auto now = OutstandingWantTracker::Clock::now();

	REQUIRE(tracker.Track("publisher/mod/7", now));
	REQUIRE_FALSE(tracker.Redeem("publisher/mod/8"));
	REQUIRE(tracker.Count() == 1);
	REQUIRE(tracker.Track("publisher/mod/8", now));
	REQUIRE_FALSE(tracker.Redeem("publisher/mod/9"));
	REQUIRE(tracker.Count() == 2);
	REQUIRE(tracker.Redeem("publisher/mod/7"));
	REQUIRE(tracker.Count() == 1);
	REQUIRE(tracker.Redeem("publisher/mod/8"));
	REQUIRE(tracker.Count() == 0);
}
