#include "downloader/announce/AnnounceWireCodec.h"

#include "domain/rules/ModNameRule.h"

#include <algorithm>
#include <string>

namespace wgrd::downloader {
void AnnounceWireCodec::AppendModName_(std::vector<std::uint8_t>& target, const std::string& modName) {
	const std::size_t copied = std::min(modName.size(), MOD_NAME_BYTES);

	for (std::size_t index = 0; index < copied; ++index) {
		target.push_back(static_cast<std::uint8_t>(modName[index]));
	}

	for (std::size_t index = copied; index < MOD_NAME_BYTES; ++index) {
		target.push_back(0);
	}
}

void AnnounceWireCodec::AppendLittleEndian64_(std::vector<std::uint8_t>& target, const std::uint64_t value) {
	for (std::size_t index = 0; index < VERSION_BYTES; ++index) {
		target.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xFF));
	}
}

std::uint64_t AnnounceWireCodec::ReadLittleEndian64_(
	const std::span<const std::uint8_t> source,
	const std::size_t offset
) {
	std::uint64_t value = 0;
	for (std::size_t index = 0; index < VERSION_BYTES; ++index) {
		value |= static_cast<std::uint64_t>(source[offset + index]) << (index * 8);
	}

	return value;
}

std::vector<std::uint8_t> AnnounceWireCodec::EncodeOffer(
	const std::span<const domain::AnnounceSummary> summaries
) {
	const std::size_t count = std::min(summaries.size(), MAXIMUM_ENTRIES);

	std::vector<std::uint8_t> message;
	message.reserve(HEADER_BYTES + COUNT_BYTES + count * OFFER_ENTRY_BYTES);

	message.push_back(static_cast<std::uint8_t>(AnnounceWireMessage::Offer));
	message.push_back(static_cast<std::uint8_t>(count & 0xFF));
	message.push_back(static_cast<std::uint8_t>((count >> 8) & 0xFF));

	for (std::size_t index = 0; index < count; ++index) {
		const domain::AnnounceSummary& summary = summaries[index];

		const auto& fingerprint = summary.publisher.Bytes();
		message.insert(message.end(), fingerprint.begin(), fingerprint.end());

		AppendModName_(message, summary.modName);
		AppendLittleEndian64_(message, summary.version);
	}

	return message;
}

std::vector<std::uint8_t> AnnounceWireCodec::EncodeWant(
	const std::span<const domain::AnnounceSummary> summaries
) {
	const std::size_t count = std::min(summaries.size(), MAXIMUM_ENTRIES);

	std::vector<std::uint8_t> message;
	message.reserve(HEADER_BYTES + COUNT_BYTES + count * WANT_ENTRY_BYTES);

	message.push_back(static_cast<std::uint8_t>(AnnounceWireMessage::Want));
	message.push_back(static_cast<std::uint8_t>(count & 0xFF));
	message.push_back(static_cast<std::uint8_t>((count >> 8) & 0xFF));

	for (std::size_t index = 0; index < count; ++index) {
		const domain::AnnounceSummary& summary = summaries[index];

		const auto& fingerprint = summary.publisher.Bytes();
		message.insert(message.end(), fingerprint.begin(), fingerprint.end());

		AppendModName_(message, summary.modName);
	}

	return message;
}

std::vector<std::uint8_t> AnnounceWireCodec::EncodeRecord(std::span<const std::uint8_t> record) {
	std::vector<std::uint8_t> message;

	if (record.size() != RECORD_BYTES) {
		return message;
	}

	message.reserve(HEADER_BYTES + RECORD_BYTES);
	message.push_back(static_cast<std::uint8_t>(AnnounceWireMessage::Record));
	message.insert(message.end(), record.begin(), record.end());

	return message;
}

std::vector<std::uint8_t> AnnounceWireCodec::EncodeAsk() {
	std::vector<std::uint8_t> message;

	message.reserve(HEADER_BYTES);
	message.push_back(static_cast<std::uint8_t>(AnnounceWireMessage::Ask));

	return message;
}

std::optional<AnnounceWireMessage> AnnounceWireCodec::MessageOf(const std::span<const std::uint8_t> message) {
	if (message.empty() || message.size() > MAXIMUM_MESSAGE_BYTES) {
		return std::nullopt;
	}

	switch (message[0]) {
		case static_cast<std::uint8_t>(AnnounceWireMessage::Offer):
			return AnnounceWireMessage::Offer;
		case static_cast<std::uint8_t>(AnnounceWireMessage::Want):
			return AnnounceWireMessage::Want;
		case static_cast<std::uint8_t>(AnnounceWireMessage::Record):
			return AnnounceWireMessage::Record;
		case static_cast<std::uint8_t>(AnnounceWireMessage::Ask):
			return AnnounceWireMessage::Ask;
		default:
			return std::nullopt;
	}
}

std::optional<std::vector<domain::AnnounceSummary>> AnnounceWireCodec::DecodeEntries_(
	const std::span<const std::uint8_t> message,
	const AnnounceWireMessage expected,
	const std::size_t entryBytes,
	const bool withVersion
) {
	const auto kind = MessageOf(message);
	if (!kind.has_value() || *kind != expected) {
		return std::nullopt;
	}

	if (message.size() < HEADER_BYTES + COUNT_BYTES) {
		return std::nullopt;
	}

	const std::size_t count =
			static_cast<std::size_t>(message[1]) |
			(static_cast<std::size_t>(message[2]) << 8);

	if (count > MAXIMUM_ENTRIES) {
		return std::nullopt;
	}

	if (message.size() != HEADER_BYTES + COUNT_BYTES + count * entryBytes) {
		return std::nullopt;
	}

	std::vector<domain::AnnounceSummary> summaries;
	summaries.reserve(count);

	for (std::size_t index = 0; index < count; ++index) {
		const std::size_t offset = HEADER_BYTES + COUNT_BYTES + index * entryBytes;

		domain::AnnounceSummary summary;

		const auto fingerprint = domain::PublisherFingerprint::FromBytes(
			message.subspan(offset, FINGERPRINT_BYTES)
		);

		if (!fingerprint.has_value()) {
			return std::nullopt;
		}

		summary.publisher = *fingerprint;

		const std::size_t nameOffset = offset + FINGERPRINT_BYTES;

		std::string modName;
		for (std::size_t position = 0; position < MOD_NAME_BYTES; ++position) {
			const std::uint8_t value = message[nameOffset + position];
			if (value == 0) {
				break;
			}
			modName.push_back(static_cast<char>(value));
		}

		if (!domain::ModNameRule::IsAcceptable(modName)) {
			return std::nullopt;
		}

		summary.modName = std::move(modName);
		summary.version = withVersion
		                  ? ReadLittleEndian64_(message, nameOffset + MOD_NAME_BYTES)
		                  : 0;

		summaries.push_back(std::move(summary));
	}

	return summaries;
}

std::optional<std::vector<domain::AnnounceSummary>> AnnounceWireCodec::DecodeOffer(
	const std::span<const std::uint8_t> message
) {
	return DecodeEntries_(message, AnnounceWireMessage::Offer, OFFER_ENTRY_BYTES, true);
}

std::optional<std::vector<domain::AnnounceSummary>> AnnounceWireCodec::DecodeWant(
	const std::span<const std::uint8_t> message
) {
	return DecodeEntries_(message, AnnounceWireMessage::Want, WANT_ENTRY_BYTES, false);
}

std::optional<std::vector<std::uint8_t>> AnnounceWireCodec::DecodeRecord(
	std::span<const std::uint8_t> message
) {
	const auto kind = MessageOf(message);
	if (!kind.has_value() || *kind != AnnounceWireMessage::Record) {
		return std::nullopt;
	}

	if (message.size() != HEADER_BYTES + RECORD_BYTES) {
		return std::nullopt;
	}

	return std::vector<std::uint8_t>(message.begin() + HEADER_BYTES, message.end());
}

std::optional<domain::AnnounceSummary> AnnounceWireCodec::RecordSummary(
	const std::span<const std::uint8_t> record
) {
	if (record.size() != RECORD_BYTES) {
		return std::nullopt;
	}

	const auto fingerprint = domain::PublisherFingerprint::FromBytes(
		record.subspan(RECORD_FINGERPRINT_OFFSET, FINGERPRINT_BYTES)
	);

	if (!fingerprint.has_value()) {
		return std::nullopt;
	}

	std::string modName;
	for (std::size_t position = 0; position < MOD_NAME_BYTES; ++position) {
		const std::uint8_t value = record[RECORD_MOD_NAME_OFFSET + position];
		if (value == 0) {
			break;
		}
		modName.push_back(static_cast<char>(value));
	}

	if (!domain::ModNameRule::IsAcceptable(modName)) {
		return std::nullopt;
	}

	domain::AnnounceSummary summary;
	summary.publisher = *fingerprint;
	summary.modName = std::move(modName);
	summary.version = ReadLittleEndian64_(record, RECORD_VERSION_OFFSET);

	return summary;
}
}
