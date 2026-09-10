#include "manager/baseline/records/BaselineRecordCodec.h"

#include "manager/payload/PayloadPathPolicy.h"

#include <nlohmann/json.hpp>

#include <limits>

namespace wgrd::manager {
std::string BaselineRecordCodec::Encode(const BaselineFileRecord& record) {
	nlohmann::json chunks = nlohmann::json::array();

	for (const domain::ManifestChunk& chunk : record.chunks) {
		nlohmann::json entry;
		entry["digest"] = chunk.digest.ToHex();
		entry["offset"] = chunk.offset;
		entry["length"] = chunk.length;
		chunks.push_back(std::move(entry));
	}

	nlohmann::json document;
	document["path"] = record.relativePath;
	document["size"] = record.stamp.size;
	document["modified"] = record.stamp.modifiedTicks;
	document["chunks"] = std::move(chunks);

	return document.dump();
}

std::optional<BaselineFileRecord> BaselineRecordCodec::Decode(const std::string_view document) {
	if (document.size() > DOCUMENT_LIMIT) {
		return std::nullopt;
	}

	const nlohmann::json parsed = nlohmann::json::parse(document, nullptr, false);

	if (parsed.is_discarded() || !parsed.is_object()) {
		return std::nullopt;
	}

	if (!parsed.contains("path") || !parsed["path"].is_string()) {
		return std::nullopt;
	}

	if (!parsed.contains("size") || !parsed["size"].is_number_unsigned()) {
		return std::nullopt;
	}

	if (!parsed.contains("modified") || !parsed["modified"].is_number_integer()) {
		return std::nullopt;
	}

	if (!parsed.contains("chunks") || !parsed["chunks"].is_array()) {
		return std::nullopt;
	}

	if (parsed["chunks"].size() > CHUNK_LIMIT) {
		return std::nullopt;
	}

	const PayloadPathPolicy policy;
	const std::string relativePath = parsed["path"].get<std::string>();
	const auto normalised = policy.Normalise(relativePath);
	if (!normalised.has_value() || *normalised != relativePath
	    || relativePath.size() < PayloadPathPolicy::PAYLOAD_EXTENSION.size()) {
		return std::nullopt;
	}

	std::string extension = relativePath.substr(relativePath.size() - PayloadPathPolicy::PAYLOAD_EXTENSION.size());
	for (char& character : extension) {
		if (character >= 'A' && character <= 'Z') {
			character = static_cast<char>(character + ('a' - 'A'));
		}
	}

	if (extension != PayloadPathPolicy::PAYLOAD_EXTENSION) {
		return std::nullopt;
	}

	if (parsed["modified"].is_number_unsigned()
	    && parsed["modified"].get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
		return std::nullopt;
	}

	BaselineFileRecord record;
	record.relativePath = parsed["path"].get<std::string>();
	record.stamp.size = parsed["size"].get<std::uint64_t>();
	record.stamp.modifiedTicks = parsed["modified"].get<std::int64_t>();
	record.chunks.reserve(parsed["chunks"].size());

	std::uint64_t covered = 0;

	for (const nlohmann::json& entry : parsed["chunks"]) {
		if (!entry.is_object()) {
			return std::nullopt;
		}

		if (!entry.contains("digest") || !entry["digest"].is_string()) {
			return std::nullopt;
		}

		if (!entry.contains("offset") || !entry["offset"].is_number_unsigned()) {
			return std::nullopt;
		}

		if (!entry.contains("length") || !entry["length"].is_number_unsigned()) {
			return std::nullopt;
		}

		const auto digest = domain::ChunkDigest::FromHex(entry["digest"].get<std::string>());
		if (!digest.has_value()) {
			return std::nullopt;
		}

		const std::uint64_t offset = entry["offset"].get<std::uint64_t>();
		const std::uint64_t length = entry["length"].get<std::uint64_t>();
		if (offset != covered || length == 0 || length > std::numeric_limits<std::uint32_t>::max()
		    || length > record.stamp.size - covered) {
			return std::nullopt;
		}

		record.chunks.push_back(domain::ManifestChunk{*digest, offset, static_cast<std::uint32_t>(length)});
		covered += length;
	}

	if (covered != record.stamp.size) {
		return std::nullopt;
	}

	return record;
}
}
