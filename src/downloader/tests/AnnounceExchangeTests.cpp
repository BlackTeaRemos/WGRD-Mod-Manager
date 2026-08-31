#include "downloader/announce/AnnounceExchange.h"
#include "downloader/announce/AnnounceWireCodec.h"
#include "downloader/announce/PeerAnnounceBudget.h"

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using wgrd::domain::AnnounceRejection;
using wgrd::domain::AnnounceSummary;
using wgrd::domain::IAnnounceCatalogue;
using wgrd::domain::IAnnounceReceiver;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::SignedAnnounce;
using wgrd::downloader::AnnounceExchange;
using wgrd::downloader::AnnounceWireCodec;
using wgrd::downloader::PeerAnnounceBudget;

namespace {
PublisherFingerprint MakeFingerprint(const std::uint8_t seed) {
	const std::array<std::uint8_t, 8> raw = {seed, 2, 3, 4, 5, 6, 7, 8};

	const auto fingerprint = PublisherFingerprint::FromBytes(raw);
	REQUIRE(fingerprint.has_value());

	return *fingerprint;
}

class HighWaterCatalogue final : public IAnnounceCatalogue {
public:
	HighWaterCatalogue() = default;

	~HighWaterCatalogue() override = default;

	void Hold(const AnnounceSummary& summary) {
		_summaries.push_back(summary);
	}

	[[nodiscard]] std::vector<AnnounceSummary> Summaries() const override {
		return _summaries;
	}

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Record(
		const PublisherFingerprint&,
		const std::string_view
	) const override {
		return std::nullopt;
	}

	[[nodiscard]] bool WouldAccept(
		const PublisherFingerprint& publisher,
		const std::string_view modName,
		const std::uint64_t version
	) const override {
		for (const AnnounceSummary& summary : _summaries) {
			if (summary.publisher == publisher && summary.modName == modName) {
				return version > summary.version;
			}
		}

		return true;
	}

private:
	std::vector<AnnounceSummary> _summaries;
};

class CountingReceiver final : public IAnnounceReceiver {
public:
	CountingReceiver() = default;

	~CountingReceiver() override = default;

	[[nodiscard]] std::expected<SignedAnnounce, AnnounceRejection> Accept(
		std::span<const std::uint8_t>
	) override {
		++_accepted;

		return SignedAnnounce{};
	}

	[[nodiscard]] std::size_t Accepted() const {
		return _accepted;
	}

private:
	std::size_t _accepted = 0;
};

std::vector<std::uint8_t> AnyRecord() {
	return std::vector<std::uint8_t>(AnnounceWireCodec::RECORD_BYTES, 0xAB);
}
}

TEST_CASE("rollback offer yields no wants") {
	HighWaterCatalogue catalogue;
	CountingReceiver receiver;

	const AnnounceSummary held{MakeFingerprint(0x51), "angel_maps", 9};
	catalogue.Hold(held);

	AnnounceExchange exchange(catalogue, receiver);

	const AnnounceSummary stale{MakeFingerprint(0x51), "angel_maps", 4};
	const std::vector<AnnounceSummary> offered = {stale};

	REQUIRE(exchange.Missing(offered).empty());

	const AnnounceSummary newer{MakeFingerprint(0x51), "angel_maps", 12};
	const std::vector<AnnounceSummary> fresher = {newer};

	REQUIRE(exchange.Missing(fresher).size() == 1);
}

TEST_CASE("single overage does not block through exchange") {
	HighWaterCatalogue catalogue;
	CountingReceiver receiver;

	AnnounceExchange exchange(catalogue, receiver);

	const std::string peer = "10.0.0.1";
	const std::vector<std::uint8_t> record = AnyRecord();

	for (std::uint32_t index = 0; index < PeerAnnounceBudget::BURST_ALLOWANCE; ++index) {
		REQUIRE(exchange.Ingest(peer, record));
	}

	REQUIRE_FALSE(exchange.Ingest(peer, record));
	REQUIRE_FALSE(exchange.PeerBlocked(peer));
	REQUIRE(exchange.Snapshot().peersThrottled == 1);
}

TEST_CASE("sustained overage blocks through exchange") {
	HighWaterCatalogue catalogue;
	CountingReceiver receiver;

	AnnounceExchange exchange(catalogue, receiver);

	const std::string peer = "10.0.0.2";
	const std::vector<std::uint8_t> record = AnyRecord();

	for (std::uint32_t index = 0; index < PeerAnnounceBudget::BURST_ALLOWANCE; ++index) {
		REQUIRE(exchange.Ingest(peer, record));
	}

	for (std::uint32_t index = 0; index < PeerAnnounceBudget::SUSTAINED_OVERAGE_THRESHOLD; ++index) {
		REQUIRE_FALSE(exchange.Ingest(peer, record));
	}

	REQUIRE(exchange.PeerBlocked(peer));
	REQUIRE(receiver.Accepted() == PeerAnnounceBudget::BURST_ALLOWANCE);
}

TEST_CASE("blocked peer survives eviction pressure") {
	HighWaterCatalogue catalogue;
	CountingReceiver receiver;

	AnnounceExchange exchange(catalogue, receiver);

	const std::string blockedPeer = "10.0.0.3";
	exchange.Penalise(blockedPeer);

	REQUIRE(exchange.PeerBlocked(blockedPeer));

	const std::vector<std::uint8_t> record = AnyRecord();

	for (std::size_t index = 0; index < AnnounceExchange::MAXIMUM_TRACKED_PEERS + 8; ++index) {
		const std::string peer = "10.1." + std::to_string(index / 250) + "." + std::to_string(index % 250);
		REQUIRE(exchange.Ingest(peer, record));
	}

	REQUIRE(exchange.PeerBlocked(blockedPeer));
}

TEST_CASE("full blocked table refuses new tracking") {
	HighWaterCatalogue catalogue;
	CountingReceiver receiver;

	AnnounceExchange exchange(catalogue, receiver);

	for (std::size_t index = 0; index < AnnounceExchange::MAXIMUM_TRACKED_PEERS; ++index) {
		const std::string peer = "10.2." + std::to_string(index / 250) + "." + std::to_string(index % 250);
		exchange.Penalise(peer);
	}

	const std::string newcomer = "10.0.0.4";
	const std::vector<std::uint8_t> record = AnyRecord();

	REQUIRE_FALSE(exchange.Ingest(newcomer, record));
	REQUIRE_FALSE(exchange.PeerBlocked(newcomer));
	REQUIRE(receiver.Accepted() == 0);
	REQUIRE(exchange.Snapshot().peersThrottled == 1);
}
