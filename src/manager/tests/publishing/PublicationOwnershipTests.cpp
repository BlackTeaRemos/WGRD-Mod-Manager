#include "PublicationOwnershipFixture.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Publication records default ownership") {
    wgrd::manager::tests::PublicationOwnershipFixture fixture;
    const auto published = fixture.publisher.Publish("demo");
    REQUIRE(published.has_value());
    const auto recorded = fixture.defaultStore.Find(published->identifier);
    REQUIRE(recorded.has_value());
    REQUIRE(recorded->modName == published->modName);
    REQUIRE(recorded->version == published->version);
    REQUIRE(recorded->manifestDigest == published->manifestDigest);
}

TEST_CASE("Publication uses explicit store") {
    wgrd::manager::tests::PublicationOwnershipFixture fixture(true);
    const auto published = fixture.publisher.Publish("demo");
    REQUIRE(published.has_value());
    REQUIRE(fixture.customStore.Find(published->identifier).has_value());
    REQUIRE_FALSE(fixture.defaultStore.Find(published->identifier).has_value());
}

TEST_CASE("Ownership write failure rejected") {
    wgrd::manager::tests::PublicationOwnershipFixture fixture;
    fixture.tree.WriteFile("data/installed", 1, 0);
    const auto published = fixture.publisher.Publish("demo");
    REQUIRE_FALSE(published.has_value());
    REQUIRE(published.error() == wgrd::domain::PublishError::StoreFailed);
    REQUIRE(fixture.publisher.History().empty());
}
