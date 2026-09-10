#include "manager/baseline/BaselineChunkCatalogue.h"

#include "manager/baseline/records/FileStampReader.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {
BaselineChunkCatalogue::BaselineChunkCatalogue() = default;

BaselineChunkCatalogue::~BaselineChunkCatalogue() = default;

std::optional<domain::HeldChunkLocation> BaselineChunkCatalogue::Locate(const domain::ChunkDigest& digest) const {
	const std::scoped_lock lock(_guard);

	const auto match = _locations.find(digest.ToHex());
	if (match == _locations.end()) {
		return std::nullopt;
	}

	return match->second.front();
}

void BaselineChunkCatalogue::Adopt(const std::filesystem::path& packRoot, const BaselineFileRecord& record) {
	const std::filesystem::path file = packRoot / std::filesystem::path(record.relativePath);

	const std::scoped_lock lock(_guard);
	Forget_(record.relativePath);
	_filesByPath.emplace(record.relativePath, BaselineFileEntry{record.relativePath, file, record.stamp});

	std::vector<std::string>& owned = _digestsByFile[record.relativePath];
	owned.reserve(record.chunks.size());

	for (const domain::ManifestChunk& chunk : record.chunks) {
		std::string digestHex = chunk.digest.ToHex();
		std::vector<domain::HeldChunkLocation>& locations = _locations[digestHex];

		const bool known = std::ranges::any_of(locations, [&file](const domain::HeldChunkLocation& location) {
			return location.file == file;
		});

		if (!known) {
			locations.push_back(domain::HeldChunkLocation{file, chunk.offset, chunk.length});
			owned.push_back(std::move(digestHex));
		}
	}
}

void BaselineChunkCatalogue::Forget_(const std::string_view relativePath) {
	const auto entry = _filesByPath.find(relativePath);
	if (entry == _filesByPath.end()) {
		return;
	}

	const auto owned = _digestsByFile.find(relativePath);
	if (owned != _digestsByFile.end()) {
		for (const std::string& digestHex : owned->second) {
			const auto locations = _locations.find(digestHex);
			if (locations == _locations.end()) {
				continue;
			}

			std::erase_if(locations->second, [&entry](const domain::HeldChunkLocation& location) {
				return location.file == entry->second.file;
			});

			if (locations->second.empty()) {
				_locations.erase(locations);
			}
		}

		_digestsByFile.erase(owned);
	}

	_filesByPath.erase(entry);
}

void BaselineChunkCatalogue::Forget(const std::string_view relativePath) {
	const std::scoped_lock lock(_guard);
	Forget_(relativePath);
}

void BaselineChunkCatalogue::Clear() {
	const std::scoped_lock lock(_guard);
	_locations.clear();
	_digestsByFile.clear();
	_filesByPath.clear();
}

void BaselineChunkCatalogue::DiscardChanged() {
	const std::scoped_lock lock(_guard);
	std::vector<std::string> changed;

	for (const auto& [relativePath, entry] : _filesByPath) {
		const std::optional<domain::FileStamp> stamp = FileStampReader::Read(entry.file);
		if (!stamp.has_value() || *stamp != entry.stamp) {
			changed.push_back(relativePath);
		}
	}

	for (const std::string& relativePath : changed) {
		Forget_(relativePath);
	}
}

std::size_t BaselineChunkCatalogue::ChunkCount() const {
	const std::scoped_lock lock(_guard);
	return _locations.size();
}
}
