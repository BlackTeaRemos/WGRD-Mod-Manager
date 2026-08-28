#include "manager/manifest/ManifestCodec.h"

#include "domain/types/distribution/TransportLimits.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace wgrd::manager {
namespace {
	using Json = nlohmann::json;

	constexpr std::string_view FIELD_PUBLISHER = "publisher";
	constexpr std::string_view FIELD_MOD = "mod";
	constexpr std::string_view FIELD_VERSION = "version";
	constexpr std::string_view FIELD_FILES = "files";
	constexpr std::string_view FIELD_PATH = "path";
	constexpr std::string_view FIELD_SIZE = "size";
	constexpr std::string_view FIELD_CHUNKS = "chunks";
	constexpr std::string_view FIELD_DIGEST = "digest";
	constexpr std::string_view FIELD_LENGTH = "length";
}

ManifestCodec::ManifestCodec(const domain::IPayloadPathPolicy& pathPolicy)
	: _pathPolicy(&pathPolicy) {}

ManifestCodec::~ManifestCodec() = default;

std::vector<std::uint8_t> ManifestCodec::Encode(const domain::ModManifest& manifest) const {
	Json document;
	document[std::string(FIELD_PUBLISHER)] = manifest.Publisher().ToHex();
	document[std::string(FIELD_MOD)] = manifest.ModName();
	document[std::string(FIELD_VERSION)] = manifest.Version();

	Json files = Json::array();
	for (const domain::ManifestFile& file : manifest.Files()) {
		Json chunks = Json::array();
		for (const domain::ManifestChunk& chunk : file.chunks) {
			Json entry;
			entry[std::string(FIELD_DIGEST)] = chunk.digest.ToHex();
			entry[std::string(FIELD_LENGTH)] = chunk.length;
			chunks.push_back(std::move(entry));
		}

		Json entry;
		entry[std::string(FIELD_PATH)] = file.path;
		entry[std::string(FIELD_SIZE)] = file.size;
		entry[std::string(FIELD_CHUNKS)] = std::move(chunks);
		files.push_back(std::move(entry));
	}

	document[std::string(FIELD_FILES)] = std::move(files);

	const std::string text = document.dump();
	return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::expected<void, domain::ManifestDecodeError> ManifestCodec::ValidateChunkLayout_(
	const domain::ManifestFile& file
) {
	std::uint64_t cursor = 0;
	for (const domain::ManifestChunk& chunk : file.chunks) {
		if (chunk.length == 0) {
			return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
		}

		if (chunk.offset != cursor) {
			return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
		}

		cursor += chunk.length;
	}

	if (cursor != file.size) {
		return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
	}

	return {};
}

std::expected<domain::ModManifest, domain::ManifestDecodeError> ManifestCodec::Decode(
	std::span<const std::uint8_t> payload
) const {
	const Json document = Json::parse(payload.begin(), payload.end(), nullptr, false);
	if (document.is_discarded() || !document.is_object()) {
		return std::unexpected(domain::ManifestDecodeError::Malformed);
	}

	for (const std::string_view field : {FIELD_PUBLISHER, FIELD_MOD, FIELD_VERSION, FIELD_FILES}) {
		if (!document.contains(std::string(field))) {
			return std::unexpected(domain::ManifestDecodeError::FieldMissing);
		}
	}

	const Json& publisherField = document.at(std::string(FIELD_PUBLISHER));
	const Json& modField = document.at(std::string(FIELD_MOD));
	const Json& versionField = document.at(std::string(FIELD_VERSION));
	const Json& filesField = document.at(std::string(FIELD_FILES));

	if (!publisherField.is_string() || !modField.is_string()) {
		return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
	}

	if (!versionField.is_number_unsigned() || !filesField.is_array()) {
		return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
	}

	const auto publisher = domain::PublisherFingerprint::FromHex(publisherField.get<std::string>());
	if (!publisher.has_value()) {
		return std::unexpected(domain::ManifestDecodeError::PublisherRejected);
	}

	if (filesField.empty()) {
		return std::unexpected(domain::ManifestDecodeError::NoFiles);
	}

	std::vector<domain::ManifestFile> files;
	files.reserve(filesField.size());

	std::size_t chunkTotal = 0;

	for (const Json& fileField : filesField) {
		if (!fileField.is_object()) {
			return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
		}

		for (const std::string_view field : {FIELD_PATH, FIELD_SIZE, FIELD_CHUNKS}) {
			if (!fileField.contains(std::string(field))) {
				return std::unexpected(domain::ManifestDecodeError::FieldMissing);
			}
		}

		const Json& pathField = fileField.at(std::string(FIELD_PATH));
		const Json& sizeField = fileField.at(std::string(FIELD_SIZE));
		const Json& chunksField = fileField.at(std::string(FIELD_CHUNKS));

		if (!pathField.is_string() || !sizeField.is_number_unsigned() || !chunksField.is_array()) {
			return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
		}

		const auto normalised = _pathPolicy->Normalise(pathField.get<std::string>());
		if (!normalised.has_value()) {
			return std::unexpected(domain::ManifestDecodeError::PathRejected);
		}

		if (sizeField.get<std::uint64_t>() > domain::limits::MANIFEST_FILE_BYTES) {
			return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
		}

		chunkTotal += chunksField.size();
		if (chunkTotal > domain::limits::MANIFEST_CHUNK_COUNT) {
			return std::unexpected(domain::ManifestDecodeError::TooManyChunks);
		}

		std::vector<domain::ManifestChunk> chunks;
		chunks.reserve(chunksField.size());

		std::uint64_t cursor = 0;
		for (const Json& chunkField : chunksField) {
			if (!chunkField.is_object()) {
				return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
			}

			if (!chunkField.contains(std::string(FIELD_DIGEST)) ||
			    !chunkField.contains(std::string(FIELD_LENGTH))) {
				return std::unexpected(domain::ManifestDecodeError::FieldMissing);
			}

			const Json& digestField = chunkField.at(std::string(FIELD_DIGEST));
			const Json& lengthField = chunkField.at(std::string(FIELD_LENGTH));

			if (!digestField.is_string() || !lengthField.is_number_unsigned()) {
				return std::unexpected(domain::ManifestDecodeError::FieldWrongType);
			}

			const auto digest = domain::ChunkDigest::FromHex(digestField.get<std::string>());
			if (!digest.has_value()) {
				return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
			}

			const std::uint64_t length = lengthField.get<std::uint64_t>();
			if (length == 0 || length > domain::limits::MANIFEST_CHUNK_LENGTH_BYTES) {
				return std::unexpected(domain::ManifestDecodeError::ChunkLayoutInvalid);
			}

			chunks.push_back(domain::ManifestChunk{
					*digest, cursor, static_cast<std::uint32_t>(length)
				}
			);

			cursor += length;
		}

		domain::ManifestFile file{
			*normalised, sizeField.get<std::uint64_t>(), std::move(chunks)
		};

		const auto layout = ValidateChunkLayout_(file);
		if (!layout.has_value()) {
			return std::unexpected(layout.error());
		}

		files.push_back(std::move(file));
	}

	return domain::ModManifest(
		*publisher,
		modField.get<std::string>(),
		versionField.get<std::uint64_t>(),
		std::move(files)
	);
}
}
