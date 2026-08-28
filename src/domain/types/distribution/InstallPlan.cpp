#include "domain/types/distribution/InstallPlan.h"

#include <utility>

namespace wgrd::domain {
InstallPlan::InstallPlan()
	: _files()
	, _removals() {}

InstallPlan::InstallPlan(std::vector<FilePlan> files, std::vector<std::string> removals)
	: _files(std::move(files))
	, _removals(std::move(removals)) {}

const std::vector<FilePlan>& InstallPlan::Files() const noexcept {
	return _files;
}

const std::vector<std::string>& InstallPlan::Removals() const noexcept {
	return _removals;
}

std::uint64_t InstallPlan::RemoteBytes() const noexcept {
	std::uint64_t total = 0;
	for (const FilePlan& file : _files) {
		for (const ChunkPlacement& placement : file.placements) {
			if (placement.source == ChunkSourceKind::Remote) {
				total += placement.length;
			}
		}
	}
	return total;
}

std::uint64_t InstallPlan::HeldBytes() const noexcept {
	std::uint64_t total = 0;
	for (const FilePlan& file : _files) {
		for (const ChunkPlacement& placement : file.placements) {
			if (placement.source == ChunkSourceKind::Held) {
				total += placement.length;
			}
		}
	}
	return total;
}

std::size_t InstallPlan::RemoteChunkCount() const noexcept {
	std::size_t total = 0;
	for (const FilePlan& file : _files) {
		for (const ChunkPlacement& placement : file.placements) {
			if (placement.source == ChunkSourceKind::Remote) {
				++total;
			}
		}
	}
	return total;
}

bool InstallPlan::IsEmpty() const noexcept {
	return _files.empty() && _removals.empty();
}
}
