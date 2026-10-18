#include "downloader/announce/AnnounceWireCodec.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using wgrd::domain::AnnounceSummary;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::AnnounceWireCodec;
using wgrd::downloader::AnnounceWireMessage;

namespace {
AnnounceSummary MakeSummary() {
	const std::array<std::uint8_t, AnnounceWireCodec::FINGERPRINT_BYTES> bytes =
		{0, 17, 34, 51, 68, 85, 102, 119};
	const auto fingerprint = PublisherFingerprint::FromBytes(bytes);
	REQUIRE(fingerprint.has_value());
	return AnnounceSummary{*fingerprint, std::string(AnnounceWireCodec::MOD_NAME_BYTES, 'a'), UINT64_MAX};
}
}

TEST_CASE("T22 truncated messages refuse") {
	const AnnounceSummary summary = MakeSummary();
	const std::vector<AnnounceSummary> summaries = {summary};
	const std::vector<std::uint8_t> offer = AnnounceWireCodec::EncodeOffer(summaries);
	const std::vector<std::uint8_t> want = AnnounceWireCodec::EncodeWant(summaries);
	const std::vector<std::uint8_t> record(AnnounceWireCodec::RECORD_BYTES, 0xAB);
	const std::vector<std::uint8_t> encodedRecord = AnnounceWireCodec::EncodeRecord(record);

	REQUIRE_FALSE(AnnounceWireCodec::DecodeOffer(std::vector<std::uint8_t>(offer.begin(), offer.end() - 1)).has_value());
	REQUIRE_FALSE(AnnounceWireCodec::DecodeWant(std::vector<std::uint8_t>(want.begin(), want.end() - 1)).has_value());
	REQUIRE_FALSE(AnnounceWireCodec::DecodeRecord(std::vector<std::uint8_t>(encodedRecord.begin(), encodedRecord.end() - 1)).has_value());
	const std::vector<std::uint8_t> empty;
	REQUIRE_FALSE(AnnounceWireCodec::MessageOf(empty).has_value());
}

TEST_CASE("T22 count framing refuses") {
	const AnnounceSummary summary = MakeSummary();
	const std::vector<AnnounceSummary> summaries = {summary};
	std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(summaries);
	message[1] = 2;

	REQUIRE_FALSE(AnnounceWireCodec::DecodeOffer(message).has_value());
}

TEST_CASE("T22 entry boundary holds") {
	const AnnounceSummary summary = MakeSummary();
	std::vector<AnnounceSummary> summaries(AnnounceWireCodec::MAXIMUM_ENTRIES, summary);
	const std::vector<std::uint8_t> message = AnnounceWireCodec::EncodeOffer(summaries);
	const auto decoded = AnnounceWireCodec::DecodeOffer(message);

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->size() == AnnounceWireCodec::MAXIMUM_ENTRIES);
	REQUIRE((*decoded)[0] == summary);
	REQUIRE_FALSE(AnnounceWireCodec::DecodeOffer(
		std::vector<std::uint8_t>{static_cast<std::uint8_t>(AnnounceWireMessage::Offer), 65, 0}
	).has_value());
}

TEST_CASE("T22 boundary fields survive") {
	const AnnounceSummary summary = MakeSummary();
	const std::vector<AnnounceSummary> summaries = {summary};
	const auto decodedOffer = AnnounceWireCodec::DecodeOffer(AnnounceWireCodec::EncodeOffer(summaries));
	const auto decodedWant = AnnounceWireCodec::DecodeWant(AnnounceWireCodec::EncodeWant(summaries));

	REQUIRE(decodedOffer.has_value());
	REQUIRE(decodedWant.has_value());
	REQUIRE((*decodedOffer)[0] == summary);
	REQUIRE((*decodedWant)[0].publisher == summary.publisher);
	REQUIRE((*decodedWant)[0].modName == summary.modName);
	REQUIRE((*decodedWant)[0].version == 0);
}
