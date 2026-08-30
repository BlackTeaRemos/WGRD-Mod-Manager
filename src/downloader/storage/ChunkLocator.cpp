#include "downloader/storage/ChunkLocator.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <algorithm>

namespace wgrd::downloader {
namespace {
	bool SameLocation(const ChunkLocation& left, const ChunkLocation& right) {
		return left.offset == right.offset
		       && left.length == right.length
		       && left.file == right.file;
	}

	bool UnderFolder(const std::filesystem::path& file, const std::filesystem::path& folder) {
		const auto folderEnd = folder.end();
		auto folderPart = folder.begin();
		auto filePart = file.begin();

		while (folderPart != folderEnd) {
			if (filePart == file.end() || *filePart != *folderPart) {
				return false;
			}

			++folderPart;
			++filePart;
		}

		return true;
	}
}

ChunkLocator::ChunkLocator()
	: _guard()
	, _locations()
	, _destinations() {}

void ChunkLocator::Insert_(
	std::map<std::string, std::vector<ChunkLocation>>& table,
	std::string fileName,
	const ChunkLocation& location
) {
	std::vector<ChunkLocation>& placements = table[std::move(fileName)];

	const bool known = std::ranges::any_of(placements, [&location](const ChunkLocation& candidate) {
			return SameLocation(candidate, location);
		}
	);

	if (!known) {
		placements.push_back(location);
	}
}

std::vector<ChunkLocation> ChunkLocator::Lookup_(
	const std::map<std::string, std::vector<ChunkLocation>>& table,
	const std::string_view chunkFileName
) {
	const std::string leaf(domain::ChunkFileNaming::LeafOf(chunkFileName));

	const auto match = table.find(leaf);
	if (match == table.end()) {
		return {};
	}

	return match->second;
}

void ChunkLocator::RegisterFile(
	std::string fileName,
	const std::filesystem::path& file,
	const std::uint64_t offset,
	const std::uint64_t length
) {
	const ChunkLocation location{file, offset, static_cast<std::uint32_t>(length)};

	const std::scoped_lock lock(_guard);

	Insert_(_locations, std::move(fileName), location);
}

void ChunkLocator::Register(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder
) {
	for (const domain::ManifestFile& file : manifest.Files()) {
		for (const domain::ManifestChunk& chunk : file.chunks) {
			RegisterFile(
				domain::ChunkFileNaming::FileNameFor(chunk.digest),
				modFolder / file.path,
				chunk.offset,
				chunk.length
			);
		}
	}
}

void ChunkLocator::Forget(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder
) {
	const std::scoped_lock lock(_guard);

	for (const domain::ManifestFile& file : manifest.Files()) {
		for (const domain::ManifestChunk& chunk : file.chunks) {
			const auto match = _locations.find(domain::ChunkFileNaming::FileNameFor(chunk.digest));
			if (match == _locations.end()) {
				continue;
			}

			std::vector<ChunkLocation>& placements = match->second;

			const auto removed = std::ranges::remove_if(
				placements,
				[&modFolder](const ChunkLocation& candidate) {
					return UnderFolder(candidate.file, modFolder);
				}
			);

			placements.erase(removed.begin(), removed.end());

			if (placements.empty()) {
				_locations.erase(match);
			}
		}
	}
}

std::optional<ChunkLocation> ChunkLocator::Find(const std::string_view chunkFileName) const {
	const std::scoped_lock lock(_guard);

	const std::vector<ChunkLocation> placements = Lookup_(_locations, chunkFileName);
	if (placements.empty()) {
		return std::nullopt;
	}

	return placements.front();
}

std::size_t ChunkLocator::Count() const {
	const std::scoped_lock lock(_guard);
	return _locations.size();
}

void ChunkLocator::RegisterDestination(
	std::string fileName,
	const std::filesystem::path& file,
	const std::uint64_t offset,
	const std::uint64_t length
) {
	const ChunkLocation location{file, offset, static_cast<std::uint32_t>(length)};

	const std::scoped_lock lock(_guard);

	Insert_(_destinations, std::move(fileName), location);
}

std::vector<ChunkLocation> ChunkLocator::FindDestinations(const std::string_view chunkFileName) const {
	const std::scoped_lock lock(_guard);

	return Lookup_(_destinations, chunkFileName);
}

bool ChunkLocator::HasDestination(const std::string_view chunkFileName) const {
	const std::scoped_lock lock(_guard);

	return !Lookup_(_destinations, chunkFileName).empty();
}

std::size_t ChunkLocator::DestinationCount() const {
	const std::scoped_lock lock(_guard);
	return _destinations.size();
}

void ChunkLocator::ClearDestinations() {
	const std::scoped_lock lock(_guard);
	_destinations.clear();
}

void ChunkLocator::SetVerifyExisting(const bool verify) {
	const std::scoped_lock lock(_guard);
	_verifyExisting = verify;
}

bool ChunkLocator::VerifyExisting() const {
	const std::scoped_lock lock(_guard);
	return _verifyExisting;
}
}
