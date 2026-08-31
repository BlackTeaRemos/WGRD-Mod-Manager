#pragma once

#include "domain/types/content/ModManifest.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

	bool RegisterFile(
		std::string fileName,
		const std::filesystem::path& file,
		std::uint64_t offset,
		std::uint64_t length
	);

	void Forget(const domain::ModManifest& manifest, const std::filesystem::path& modFolder);

	[[nodiscard]] std::optional<ChunkLocation> Find(std::string_view chunkFileName) const;

	[[nodiscard]] std::size_t Count() const;

	bool RegisterDestination(
		std::string fileName,
		const std::filesystem::path& file,
		std::uint64_t offset,
		std::uint64_t length
	);

	[[nodiscard]] std::vector<ChunkLocation> FindDestinations(std::string_view chunkFileName) const;

	[[nodiscard]] bool HasDestination(std::string_view chunkFileName) const;

	[[nodiscard]] std::size_t DestinationCount() const;

	void ClearDestinations();

	void SetFetchStaging(std::filesystem::path stagingFolder);

	[[nodiscard]] bool IsFetchStaging(const std::filesystem::path& savePath) const;

	void SetVerifyExisting(bool verify);

	[[nodiscard]] bool VerifyExisting() const;

private:
	[[nodiscard]] static std::vector<ChunkLocation> Lookup_(
		const std::map<std::string, std::vector<ChunkLocation>>& table,
		std::string_view chunkFileName
	);

	static void Insert_(
		std::map<std::string, std::vector<ChunkLocation>>& table,
		std::string fileName,
		const ChunkLocation& location
	);

	mutable std::mutex _guard;
	std::filesystem::path _fetchStaging;
	bool _verifyExisting = false;
	std::map<std::string, std::vector<ChunkLocation>> _locations;
	std::map<std::string, std::vector<ChunkLocation>> _destinations;
};
}
