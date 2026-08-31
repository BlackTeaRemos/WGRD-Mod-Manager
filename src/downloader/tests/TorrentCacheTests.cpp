#include "downloader/torrent/build/TorrentCache.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using wgrd::downloader::TorrentCache;
using wgrd::downloader::TorrentCacheError;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-torrent-cache" / label;

		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
		std::filesystem::create_directories(_root, failure);
	}

	~TemporaryTree() {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}

	[[nodiscard]] const std::filesystem::path& Root() const {
		return _root;
	}

private:
	std::filesystem::path _root;
};

std::vector<char> MakePayload(const std::size_t bytes) {
	std::vector<char> payload(bytes);

	for (std::size_t index = 0; index < bytes; ++index) {
		payload[index] = static_cast<char>(((index * 131) ^ (index >> 7) ^ 0x5C) & 0xFF);
	}

	return payload;
}

std::size_t CountEntries(const std::filesystem::path& folder) {
	std::error_code failure;
	std::size_t total = 0;

	for (const std::filesystem::directory_entry& entry
	     : std::filesystem::directory_iterator(folder, failure)) {
		(void)entry;
		total += 1;
	}

	return total;
}
}

TEST_CASE("torrent cache round trips a saved payload") {
	const TemporaryTree tree("round-trip");
	const TorrentCache cache(tree.Root());

	const std::vector<char> payload = MakePayload(4096);

	const auto saved = cache.Save("ab12cd34", payload);
	REQUIRE(saved.has_value());

	const auto loaded = cache.Load("ab12cd34");
	REQUIRE(loaded.has_value());
	REQUIRE(*loaded == payload);
}

TEST_CASE("torrent cache reports a missing entry") {
	const TemporaryTree tree("missing");
	const TorrentCache cache(tree.Root());

	const auto loaded = cache.Load("ab12cd34");

	REQUIRE_FALSE(loaded.has_value());
	REQUIRE(loaded.error() == TorrentCacheError::EntryMissing);
}

TEST_CASE("torrent cache refuses an oversized entry without reading it") {
	const TemporaryTree tree("oversized");
	const TorrentCache cache(tree.Root());

	const std::filesystem::path oversized = tree.Root() / "ab12cd34.torrent";

	{
		std::ofstream output(oversized, std::ios::binary | std::ios::trunc);
		output.seekp(static_cast<std::streamoff>(TorrentCache::MAXIMUM_BYTES));
		output.put('x');
	}

	REQUIRE(std::filesystem::file_size(oversized) == TorrentCache::MAXIMUM_BYTES + 1);

	const auto loaded = cache.Load("ab12cd34");

	REQUIRE_FALSE(loaded.has_value());
	REQUIRE(loaded.error() == TorrentCacheError::EntryOversized);
}

TEST_CASE("torrent cache rejects an unacceptable key observably") {
	const TemporaryTree tree("bad-key");
	const TorrentCache cache(tree.Root());

	const std::vector<char> payload = MakePayload(64);

	const auto saved = cache.Save("manifest", payload);
	REQUIRE_FALSE(saved.has_value());
	REQUIRE(saved.error() == TorrentCacheError::KeyRejected);

	const auto loaded = cache.Load("manifest");
	REQUIRE_FALSE(loaded.has_value());
	REQUIRE(loaded.error() == TorrentCacheError::KeyRejected);

	const auto traversal = cache.Load("../ab12cd34");
	REQUIRE_FALSE(traversal.has_value());
	REQUIRE(traversal.error() == TorrentCacheError::KeyRejected);

	REQUIRE(CountEntries(tree.Root()) == 0);
}

TEST_CASE("torrent cache rejects an empty payload") {
	const TemporaryTree tree("empty-payload");
	const TorrentCache cache(tree.Root());

	const auto saved = cache.Save("ab12cd34", {});

	REQUIRE_FALSE(saved.has_value());
	REQUIRE(saved.error() == TorrentCacheError::PayloadRejected);
}

TEST_CASE("torrent cache prunes only dead keys") {
	const TemporaryTree tree("prune");
	const TorrentCache cache(tree.Root());

	const std::vector<char> payload = MakePayload(256);

	REQUIRE(cache.Save("aa11", payload).has_value());
	REQUIRE(cache.Save("bb22", payload).has_value());
	REQUIRE(cache.Save("cc33", payload).has_value());

	{
		std::ofstream foreign(tree.Root() / "notes.txt", std::ios::binary | std::ios::trunc);
		foreign.put('n');
	}

	const std::size_t removed = cache.Prune({"aa11"});

	REQUIRE(removed == 2);
	REQUIRE(std::filesystem::exists(tree.Root() / "aa11.torrent"));
	REQUIRE_FALSE(std::filesystem::exists(tree.Root() / "bb22.torrent"));
	REQUIRE_FALSE(std::filesystem::exists(tree.Root() / "cc33.torrent"));
	REQUIRE(std::filesystem::exists(tree.Root() / "notes.txt"));

	REQUIRE(cache.Load("aa11").has_value());
}

TEST_CASE("torrent cache leaves no staging file after save") {
	const TemporaryTree tree("staging");
	const TorrentCache cache(tree.Root());

	const std::vector<char> payload = MakePayload(1024);

	REQUIRE(cache.Save("dd44ee55", payload).has_value());

	std::error_code failure;
	std::size_t stagingCount = 0;
	std::size_t entryCount = 0;

	for (const std::filesystem::directory_entry& entry
	     : std::filesystem::directory_iterator(tree.Root(), failure)) {
		entryCount += 1;

		const std::string name = entry.path().filename().string();

		if (name.find(std::string(TorrentCache::STAGE_MARK)) != std::string::npos) {
			stagingCount += 1;
		}
	}

	REQUIRE(entryCount == 1);
	REQUIRE(stagingCount == 0);
	REQUIRE(std::filesystem::exists(tree.Root() / "dd44ee55.torrent"));
}
