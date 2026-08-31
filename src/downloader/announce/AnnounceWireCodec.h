#pragma once

#include "domain/types/distribution/AnnounceSummary.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace wgrd::downloader {
enum class AnnounceWireMessage : std::uint8_t {
	Offer = 0x01, Want = 0x02, Record = 0x03
};

class AnnounceWireCodec {
public:
	static constexpr std::size_t MOD_NAME_BYTES = 64;
	static constexpr std::size_t FINGERPRINT_BYTES = 8;
	static constexpr std::size_t VERSION_BYTES = 8;

	static constexpr std::size_t OFFER_ENTRY_BYTES =
			FINGERPRINT_BYTES + MOD_NAME_BYTES + VERSION_BYTES;

	static constexpr std::size_t WANT_ENTRY_BYTES = FINGERPRINT_BYTES + MOD_NAME_BYTES;

	static constexpr std::size_t RECORD_BYTES = 220;
	static constexpr std::size_t RECORD_FINGERPRINT_OFFSET = 12;
	static constexpr std::size_t RECORD_MOD_NAME_OFFSET = 20;
	static constexpr std::size_t RECORD_VERSION_OFFSET = 84;

	static constexpr std::size_t MAXIMUM_ENTRIES = 64;
	static constexpr std::size_t MAXIMUM_MESSAGE_BYTES = 8192;

	static constexpr std::size_t HEADER_BYTES = 1;
	static constexpr std::size_t COUNT_BYTES = 2;

	[[nodiscard]] static std::vector<std::uint8_t> EncodeOffer(
		std::span<const domain::AnnounceSummary> summaries
	);

	[[nodiscard]] static std::vector<std::uint8_t> EncodeWant(
		std::span<const domain::AnnounceSummary> summaries
	);

	[[nodiscard]] static std::vector<std::uint8_t> EncodeRecord(
		std::span<const std::uint8_t> record
	);

	[[nodiscard]] static std::optional<AnnounceWireMessage> MessageOf(
		std::span<const std::uint8_t> message
	);

	[[nodiscard]] static std::optional<std::vector<domain::AnnounceSummary>> DecodeOffer(
		std::span<const std::uint8_t> message
	);

	[[nodiscard]] static std::optional<std::vector<domain::AnnounceSummary>> DecodeWant(
		std::span<const std::uint8_t> message
	);

	[[nodiscard]] static std::optional<std::vector<std::uint8_t>> DecodeRecord(
		std::span<const std::uint8_t> message
	);

	[[nodiscard]] static std::optional<domain::AnnounceSummary> RecordSummary(
		std::span<const std::uint8_t> record
	);

private:
	AnnounceWireCodec() = delete;

	static void AppendModName_(std::vector<std::uint8_t>& target, const std::string& modName);

	static void AppendLittleEndian64_(std::vector<std::uint8_t>& target, std::uint64_t value);

	[[nodiscard]] static std::uint64_t ReadLittleEndian64_(
		std::span<const std::uint8_t> source,
		std::size_t offset
	);

	[[nodiscard]] static std::optional<std::vector<domain::AnnounceSummary>> DecodeEntries_(
		std::span<const std::uint8_t> message,
		AnnounceWireMessage expected,
		std::size_t entryBytes,
		bool withVersion
	);
};
}
