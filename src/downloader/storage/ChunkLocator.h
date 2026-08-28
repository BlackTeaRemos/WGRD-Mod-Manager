#pragma once

#include "domain/types/content/ModManifest.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace wgrd::downloader {
struct ChunkLocation {
	std::filesystem::path file;
	std::uint64_t offset;
	std::uint32_t length;
};

class ChunkLocator {
public:
	ChunkLocator();

	void Register(const domain::ModManifest& manifest, const std::filesystem::path& modFolder);

	void RegisterFile(
		std::string fileName,
		const std::filesystem::path& file,
		std::uint64_t offset,
		std::uint64_t length
	);

	void Forget(const domain::ModManifest& manifest);

	[[nodiscard]] std::optional<ChunkLocation> Find(std::string_view chunkFileName) const;

	[[nodiscard]] std::size_t Count() const;

private:
	mutable std::mutex _guard;
	std::map<std::string, ChunkLocation> _locations;
};
}
