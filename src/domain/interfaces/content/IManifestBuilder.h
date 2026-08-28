#pragma once

#include "domain/types/content/ModManifest.h"
#include "domain/types/identity/PublisherFingerprint.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>

namespace wgrd::domain {
enum class ManifestBuildError {
	FolderMissing
	, FolderUnreadable
	, FolderEmpty
	, PathRejected
	, FileUnreadable
	, TooManyChunks
	, ModNameRejected
};

class IManifestBuilder {
public:
	virtual ~IManifestBuilder() = 0;

	[[nodiscard]] virtual std::expected<ModManifest, ManifestBuildError> Build(
		const std::filesystem::path& modFolder,
		const PublisherFingerprint& publisher,
		std::string_view modName,
		std::uint64_t version
	) const = 0;
};

inline IManifestBuilder::~IManifestBuilder() = default;
}
