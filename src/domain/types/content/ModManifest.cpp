#include "domain/types/content/ModManifest.h"

#include <utility>

namespace wgrd::domain {
ModManifest::ModManifest()
	: _publisher()
	, _modName()
	, _version(0)
	, _files() {}

ModManifest::ModManifest(
	const PublisherFingerprint publisher,
	std::string modName,
	const std::uint64_t version,
	std::vector<ManifestFile> files,
	std::string description,
	std::shared_ptr<const ModPreview> preview
)
	: _publisher(publisher)
	, _modName(std::move(modName))
	, _description(std::move(description))
	, _preview(std::move(preview))
	, _version(version)
	, _files(std::move(files)) {}

const PublisherFingerprint& ModManifest::Publisher() const noexcept {
	return _publisher;
}

const std::string& ModManifest::ModName() const noexcept {
	return _modName;
}

std::uint64_t ModManifest::Version() const noexcept {
	return _version;
}

const std::string& ModManifest::Description() const noexcept {
	return _description;
}

std::shared_ptr<const ModPreview> ModManifest::Preview() const noexcept {
	return _preview;
}

bool ModManifest::operator==(const ModManifest& other) const {
	const bool samePreview = (!_preview && !other._preview)
		|| (_preview && other._preview && *_preview == *other._preview);
	return _publisher == other._publisher && _modName == other._modName
		&& _description == other._description && _version == other._version
		&& _files == other._files && samePreview;
}

const std::vector<ManifestFile>& ModManifest::Files() const noexcept {
	return _files;
}

std::string ModManifest::Identifier() const {
	return _publisher.ToHex() + "/" + _modName;
}

std::string ModManifest::TorrentName() const {
	return _publisher.ToHex() + "_" + _modName;
}

std::uint64_t ModManifest::TotalBytes() const noexcept {
	std::uint64_t total = 0;
	for (const ManifestFile& file : _files) {
		total += file.size;
	}
	return total;
}

std::size_t ModManifest::ChunkCount() const noexcept {
	std::size_t total = 0;
	for (const ManifestFile& file : _files) {
		total += file.chunks.size();
	}
	return total;
}
}
