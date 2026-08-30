#include "downloader/announce/AnnounceWireCodec.h"
#include "downloader/announce/ControlSwarmTorrent.h"
#include "downloader/announce/PeerAnnounceBudget.h"

#include <catch2/catch_test_macros.hpp>

#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <string>
#include <vector>

using wgrd::domain::AnnounceSummary;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::AnnounceWireCodec;
using wgrd::downloader::ControlSwarmTorrent;
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
