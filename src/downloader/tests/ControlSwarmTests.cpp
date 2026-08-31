#include "downloader/announce/AnnounceWireCodec.h"
#include "downloader/announce/ControlSwarmTorrent.h"
#include "downloader/announce/OutstandingWantTracker.h"
#include "downloader/announce/PeerAnnounceBudget.h"

#include <catch2/catch_test_macros.hpp>

#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <chrono>
#include <string>
#include <vector>

using wgrd::domain::AnnounceSummary;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::AnnounceWireCodec;
using wgrd::downloader::ControlSwarmTorrent;
using wgrd::downloader::OutstandingWantTracker;
using wgrd::downloader::PeerAnnounceBudget;

namespace {
libtorrent::add_torrent_params Load(const std::vector<char>& bencoded) {
	libtorrent::error_code parsing;

	return libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(
			bencoded.data(),
			static_cast<std::ptrdiff_t>(bencoded.size())
		),
		parsing,
		libtorrent::load_torrent_limits{}
	);
}

AnnounceSummary MakeSummary(std::string modName, const std::uint64_t version) {
	const std::array<std::uint8_t, 8> raw = {1, 2, 3, 4, 5, 6, 7, 8};

	const auto fingerprint = PublisherFingerprint::FromBytes(raw);
	REQUIRE(fingerprint.has_value());

	return AnnounceSummary{*fingerprint, std::move(modName), version};
}

std::vector<std::uint8_t> MakeIdentityRecord(const AnnounceSummary& summary) {
	std::vector<std::uint8_t> record(AnnounceWireCodec::RECORD_BYTES, 0xCD);

	const auto fingerprint = summary.publisher.Bytes();
	for (std::size_t position = 0; position < AnnounceWireCodec::FINGERPRINT_BYTES; ++position) {
		record[AnnounceWireCodec::RECORD_FINGERPRINT_OFFSET + position] = fingerprint[position];
	}

	for (std::size_t position = 0; position < AnnounceWireCodec::MOD_NAME_BYTES; ++position) {
		const std::uint8_t value = position < summary.modName.size()
		                           ? static_cast<std::uint8_t>(summary.modName[position])
		                           : 0;
		record[AnnounceWireCodec::RECORD_MOD_NAME_OFFSET + position] = value;
	}

	for (std::size_t position = 0; position < AnnounceWireCodec::VERSION_BYTES; ++position) {
		record[AnnounceWireCodec::RECORD_VERSION_OFFSET + position] =
				static_cast<std::uint8_t>((summary.version >> (position * 8)) & 0xFF);
	}

	return record;
}

void DrainAllowance(PeerAnnounceBudget& budget, const PeerAnnounceBudget::Clock::time_point now) {
	while (budget.Allowance() > 0) {
		REQUIRE(budget.Consume(now));
	}
}
}

TEST_CASE("control swarm torrent parses") {
	const ControlSwarmTorrent::Built built = ControlSwarmTorrent::Create();

	REQUIRE(built.payload.size() == ControlSwarmTorrent::PAYLOAD_BYTES);
	REQUIRE_FALSE(built.bencoded.empty());

	const libtorrent::add_torrent_params parameters = Load(built.bencoded);

	REQUIRE(parameters.ti != nullptr);
	REQUIRE(parameters.ti->num_files() == 1);
	REQUIRE(parameters.ti->total_size() == static_cast<std::int64_t>(ControlSwarmTorrent::PAYLOAD_BYTES));
	REQUIRE(parameters.ti->layout().file_path(libtorrent::file_index_t{0}) == built.name);
}

TEST_CASE("control swarm torrent is deterministic") {
	const ControlSwarmTorrent::Built first = ControlSwarmTorrent::Create();
	const ControlSwarmTorrent::Built second = ControlSwarmTorrent::Create();

	REQUIRE(first.bencoded == second.bencoded);

	const libtorrent::add_torrent_params left = Load(first.bencoded);
	const libtorrent::add_torrent_params right = Load(second.bencoded);

	REQUIRE(left.ti != nullptr);
	REQUIRE(right.ti != nullptr);
	REQUIRE(left.ti->info_hashes() == right.ti->info_hashes());
}

TEST_CASE("offer round trips") {
	const std::vector<AnnounceSummary> summaries = {
		MakeSummary("angel_maps", 3), MakeSummary("Other_mod-1", 900000)
	};

	const std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(summaries);

	const auto decoded = AnnounceWireCodec::DecodeOffer(message);

	REQUIRE(decoded.has_value());
	REQUIRE(*decoded == summaries);
}

TEST_CASE("want round trips without versions") {
	const std::vector<AnnounceSummary> summaries = {MakeSummary("angel_maps", 7)};

	const auto decoded = AnnounceWireCodec::DecodeWant(AnnounceWireCodec::EncodeWant(summaries));

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->size() == 1);
	REQUIRE((*decoded)[0].modName == "angel_maps");
	REQUIRE((*decoded)[0].version == 0);
}

TEST_CASE("record round trips") {
	const std::vector<std::uint8_t> record(AnnounceWireCodec::RECORD_BYTES, 0xAB);

	const auto decoded = AnnounceWireCodec::DecodeRecord(AnnounceWireCodec::EncodeRecord(record));

	REQUIRE(decoded.has_value());
	REQUIRE(*decoded == record);
}

TEST_CASE("offer entries are capped") {
	std::vector<AnnounceSummary> summaries;
	for (std::size_t index = 0; index < AnnounceWireCodec::MAXIMUM_ENTRIES + 20; ++index) {
		summaries.push_back(MakeSummary("mod" + std::to_string(index), index + 1));
	}

	const std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(summaries);

	REQUIRE(message.size() <= AnnounceWireCodec::MAXIMUM_MESSAGE_BYTES);

	const auto decoded = AnnounceWireCodec::DecodeOffer(message);

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->size() == AnnounceWireCodec::MAXIMUM_ENTRIES);
}

TEST_CASE("truncated offer is refused") {
	const std::vector<AnnounceSummary> summaries = {MakeSummary("angel_maps", 1)};

	std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(summaries);
	message.pop_back();

	REQUIRE_FALSE(AnnounceWireCodec::DecodeOffer(message).has_value());
}

TEST_CASE("unknown message type is refused") {
	const std::vector<std::uint8_t> message = {0x77, 0x00, 0x00};

	REQUIRE_FALSE(AnnounceWireCodec::MessageOf(message).has_value());
}

TEST_CASE("empty mod name is refused") {
	std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(
		std::vector<AnnounceSummary>{MakeSummary("angel_maps", 1)}
	);

	for (std::size_t index = 0; index < AnnounceWireCodec::MOD_NAME_BYTES; ++index) {
		message[AnnounceWireCodec::HEADER_BYTES + AnnounceWireCodec::COUNT_BYTES
		        + AnnounceWireCodec::FINGERPRINT_BYTES + index] = 0;
	}

	REQUIRE_FALSE(AnnounceWireCodec::DecodeOffer(message).has_value());
}

TEST_CASE("budget allows a burst then refuses") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	for (std::uint32_t index = 0; index < PeerAnnounceBudget::BURST_ALLOWANCE; ++index) {
		REQUIRE(budget.Consume(now));
	}

	REQUIRE_FALSE(budget.Consume(now));
}

TEST_CASE("budget refills one per interval") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	for (std::uint32_t index = 0; index < PeerAnnounceBudget::BURST_ALLOWANCE; ++index) {
		REQUIRE(budget.Consume(now));
	}

	REQUIRE(budget.Consume(now + PeerAnnounceBudget::REFILL_INTERVAL));
	REQUIRE_FALSE(budget.Consume(now + PeerAnnounceBudget::REFILL_INTERVAL));
}

TEST_CASE("penalty blocks and widens") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	budget.Penalise(now);

	REQUIRE(budget.Blocked(now));
	REQUIRE_FALSE(budget.Consume(now));

	REQUIRE_FALSE(budget.Blocked(now + std::chrono::minutes(2)));

	budget.Penalise(now + std::chrono::minutes(2));

	REQUIRE(budget.Blocked(now + std::chrono::minutes(6)));
}

TEST_CASE("single overage only drops") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	DrainAllowance(budget, now);

	REQUIRE_FALSE(budget.Consume(now));
	REQUIRE_FALSE(budget.Blocked(now));
}

TEST_CASE("sustained overage climbs the ladder") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	DrainAllowance(budget, now);

	for (std::uint32_t attempt = 0; attempt + 1 < PeerAnnounceBudget::SUSTAINED_OVERAGE_THRESHOLD; ++attempt) {
		REQUIRE_FALSE(budget.Consume(now));
		REQUIRE_FALSE(budget.Blocked(now));
	}

	REQUIRE_FALSE(budget.Consume(now));
	REQUIRE(budget.Blocked(now + std::chrono::seconds(30)));
	REQUIRE_FALSE(budget.Blocked(now + std::chrono::minutes(2)));

	const auto resumed = now + std::chrono::minutes(2);

	while (budget.Consume(resumed)) {
	}

	for (std::uint32_t attempt = 0; attempt + 1 < PeerAnnounceBudget::SUSTAINED_OVERAGE_THRESHOLD; ++attempt) {
		REQUIRE_FALSE(budget.Consume(resumed));
	}

	REQUIRE(budget.Blocked(resumed + std::chrono::minutes(4)));
	REQUIRE_FALSE(budget.Blocked(resumed + std::chrono::minutes(5) + std::chrono::seconds(1)));
}

TEST_CASE("ladder resets after one clean hour") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	budget.Penalise(now);
	budget.Penalise(now + std::chrono::minutes(2));

	REQUIRE(budget.Escalated());

	const auto cleanLater = now + std::chrono::minutes(70);

	budget.Penalise(cleanLater);

	REQUIRE(budget.Blocked(cleanLater + std::chrono::seconds(30)));
	REQUIRE_FALSE(budget.Blocked(cleanLater + std::chrono::minutes(1) + std::chrono::seconds(1)));
}

TEST_CASE("ladder holds inside a dirty hour") {
	PeerAnnounceBudget budget;
	const auto now = PeerAnnounceBudget::Clock::now();

	budget.Penalise(now);
	budget.Penalise(now + std::chrono::minutes(2));
	budget.Penalise(now + std::chrono::minutes(30));

	REQUIRE(budget.Blocked(now + std::chrono::minutes(50)));
}

TEST_CASE("want tracker refuses at ceiling") {
	OutstandingWantTracker tracker;
	const auto now = OutstandingWantTracker::Clock::now();

	for (std::size_t index = 0; index < OutstandingWantTracker::MAXIMUM_OUTSTANDING; ++index) {
		REQUIRE(tracker.Track("mod" + std::to_string(index), now));
	}

	REQUIRE_FALSE(tracker.Track("overflow", now));
	REQUIRE(tracker.Count() == OutstandingWantTracker::MAXIMUM_OUTSTANDING);
}

TEST_CASE("want tracker rejects unsolicited redeem") {
	OutstandingWantTracker tracker;
	const auto now = OutstandingWantTracker::Clock::now();

	REQUIRE(tracker.Track("wanted_mod", now));
	REQUIRE(tracker.Redeem("wanted_mod"));
	REQUIRE_FALSE(tracker.Redeem("wanted_mod"));
	REQUIRE_FALSE(tracker.Redeem("never_wanted"));
}

TEST_CASE("want tracker expires stale wants") {
	OutstandingWantTracker tracker;
	const auto now = OutstandingWantTracker::Clock::now();

	REQUIRE(tracker.Track("stale_mod", now));
	REQUIRE(tracker.Track("fresh_mod", now));
	REQUIRE(tracker.Track("fresh_mod", now + std::chrono::seconds(100)));

	tracker.Prune(now + OutstandingWantTracker::WANT_EXPIRY);

	REQUIRE_FALSE(tracker.Redeem("stale_mod"));
	REQUIRE(tracker.Redeem("fresh_mod"));
}

TEST_CASE("record identity decodes") {
	const AnnounceSummary summary = MakeSummary("angel_maps", 12);

	const auto identity = AnnounceWireCodec::RecordSummary(MakeIdentityRecord(summary));

	REQUIRE(identity.has_value());
	REQUIRE(identity->publisher == summary.publisher);
	REQUIRE(identity->modName == summary.modName);
	REQUIRE(identity->version == summary.version);
}

TEST_CASE("record identity refuses empty name") {
	const AnnounceSummary summary = MakeSummary("", 1);

	REQUIRE_FALSE(AnnounceWireCodec::RecordSummary(MakeIdentityRecord(summary)).has_value());
}

TEST_CASE("record identity refuses wrong length") {
	const std::vector<std::uint8_t> record(AnnounceWireCodec::RECORD_BYTES - 1, 0xAB);

	REQUIRE_FALSE(AnnounceWireCodec::RecordSummary(record).has_value());
}
