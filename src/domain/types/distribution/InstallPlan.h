#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wgrd::domain {
enum class ChunkSourceKind {
	Held, Remote
};

struct ChunkPlacement {
	ChunkDigest digest;
	std::uint64_t targetOffset;
	std::uint32_t length;
	ChunkSourceKind source;
	std::string heldPath;
	std::uint64_t heldOffset;

	bool operator==(const ChunkPlacement& other) const = default;
};

struct FilePlan {
	std::string path;
	std::uint64_t size;
	std::vector<ChunkPlacement> placements;

	bool operator==(const FilePlan& other) const = default;
};

class InstallPlan {
public:
	InstallPlan();

	InstallPlan(std::vector<FilePlan> files, std::vector<std::string> removals);

	[[nodiscard]] const std::vector<FilePlan>& Files() const noexcept;

	[[nodiscard]] const std::vector<std::string>& Removals() const noexcept;

	[[nodiscard]] std::uint64_t RemoteBytes() const noexcept;

	[[nodiscard]] std::uint64_t HeldBytes() const noexcept;

	[[nodiscard]] std::size_t RemoteChunkCount() const noexcept;

	[[nodiscard]] bool IsEmpty() const noexcept;

	bool operator==(const InstallPlan& other) const = default;

private:
	std::vector<FilePlan> _files;
	std::vector<std::string> _removals;
};
}
