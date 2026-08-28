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
	std::vector<ManifestFile> files
)
	: _publisher(publisher)
	, _modName(std::move(modName))
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
