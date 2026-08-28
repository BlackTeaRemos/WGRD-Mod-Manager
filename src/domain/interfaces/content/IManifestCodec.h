#pragma once

#include "domain/types/content/ModManifest.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::domain {
enum class ManifestDecodeError {
	Malformed
	, FieldMissing
	, FieldWrongType
	, PublisherRejected
	, ModNameRejected
	, PathRejected
	, TooManyChunks
	, ChunkLayoutInvalid
	, NoFiles
};

class IManifestCodec {
public:
	virtual ~IManifestCodec() = 0;

	[[nodiscard]] virtual std::vector<std::uint8_t> Encode(const ModManifest& manifest) const = 0;

	[[nodiscard]] virtual std::expected<ModManifest, ManifestDecodeError> Decode(
		std::span<const std::uint8_t> payload
	) const = 0;
};

inline IManifestCodec::~IManifestCodec() = default;
}
