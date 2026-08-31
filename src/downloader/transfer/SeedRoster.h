#pragma once

#include "domain/types/content/ModManifest.h"
#include "domain/types/status/SeedEntry.h"

#include <libtorrent/torrent_handle.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::downloader {
struct SeededTorrent {
	std::string identifier;
	std::shared_ptr<libtorrent::torrent_handle> handle;
	domain::ModManifest manifest;
	std::filesystem::path modFolder;
};

struct SeedHandleView {
	std::string identifier;
	std::shared_ptr<libtorrent::torrent_handle> handle;
	std::uint32_t peers;
};

struct RemovedSeed {
	std::shared_ptr<libtorrent::torrent_handle> handle;
	domain::ModManifest manifest;
	std::filesystem::path modFolder;
	std::string infoHash;
};

class SeedRoster {
public:
	explicit SeedRoster();

	SeedRoster(const SeedRoster&) = delete;

	SeedRoster& operator=(const SeedRoster&) = delete;

	[[nodiscard]] bool Contains(std::string_view identifier) const;

	[[nodiscard]] bool Add(SeededTorrent seeded, domain::SeedEntry entry);

	[[nodiscard]] std::optional<RemovedSeed> Remove(std::string_view identifier);

	[[nodiscard]] std::vector<SeedHandleView> Handles() const;

	[[nodiscard]] std::shared_ptr<libtorrent::torrent_handle> HandleFor(
		std::string_view identifier
	) const;

	[[nodiscard]] std::vector<domain::SeedEntry> Snapshot() const;

	[[nodiscard]] std::uint64_t UploadedBytes() const;

	void Update(
		std::string_view identifier,
		bool seeding,
		std::uint32_t peers,
		std::uint64_t uploadedBytes
	);

private:
	[[nodiscard]] std::size_t PositionOf_(std::string_view identifier) const;

	mutable std::mutex _guard;
	std::vector<SeededTorrent> _seeded;
	std::vector<domain::SeedEntry> _entries;
};
}
