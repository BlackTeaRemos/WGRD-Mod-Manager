#include "downloader/storage/ChunkLocator.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <algorithm>

namespace wgrd::downloader {
namespace {
	bool SameLocation(const ChunkLocation& left, const ChunkLocation& right) {
		return left.file == right.file
		       && left.offset == right.offset
		       && left.length == right.length;
	}
}

ChunkLocator::ChunkLocator()
	: _guard()
	, _locations() {}

void ChunkLocator::RegisterFile(
	std::string fileName,
	const std::filesystem::path& file,
	const std::uint64_t offset,
	const std::uint64_t length
) {
	const ChunkLocation location{file, offset, static_cast<std::uint32_t>(length)};

	const std::scoped_lock lock(_guard);

	std::vector<ChunkLocation>& placements = _locations[std::move(fileName)];

	const bool known = std::ranges::any_of(placements, [&location](const ChunkLocation& candidate) {
			return SameLocation(candidate, location);
		}
	);

	if (!known) {
		placements.push_back(location);
	}
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

void ChunkLocator::Forget(const domain::ModManifest& manifest) {
	const std::scoped_lock lock(_guard);

	for (const domain::ManifestFile& file : manifest.Files()) {
		for (const domain::ManifestChunk& chunk : file.chunks) {
			_locations.erase(domain::ChunkFileNaming::FileNameFor(chunk.digest));
		}
	}
}

void ChunkLocator::ForgetFile(const std::string_view fileName) {
	const std::scoped_lock lock(_guard);

	_locations.erase(std::string(domain::ChunkFileNaming::LeafOf(fileName)));
}

std::optional<ChunkLocation> ChunkLocator::Find(const std::string_view chunkFileName) const {
	const std::string leaf(domain::ChunkFileNaming::LeafOf(chunkFileName));

	const std::scoped_lock lock(_guard);

	const auto match = _locations.find(leaf);
	if (match == _locations.end() || match->second.empty()) {
		return std::nullopt;
	}

	return match->second.front();
}

std::vector<ChunkLocation> ChunkLocator::FindAll(const std::string_view chunkFileName) const {
	const std::string leaf(domain::ChunkFileNaming::LeafOf(chunkFileName));

	const std::scoped_lock lock(_guard);

	const auto match = _locations.find(leaf);
	if (match == _locations.end()) {
		return {};
	}

	return match->second;
}

std::size_t ChunkLocator::Count() const {
	const std::scoped_lock lock(_guard);
	return _locations.size();
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
