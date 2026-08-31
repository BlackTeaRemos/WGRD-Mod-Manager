#include "downloader/storage/InstalledFolderStorage.h"

#include <catch2/catch_test_macros.hpp>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/peer_request.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::downloader::ChunkLocator;
using wgrd::downloader::InstalledFolderStorage;
using wgrd::downloader::OpenFileCache;
using wgrd::downloader::SeedAttestations;
using wgrd::downloader::StorageBacklog;
using wgrd::downloader::StorageFaults;

namespace {
constexpr int PIECE_BYTES = 16384;
constexpr std::uint32_t CHUNK_BYTES = 300;

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-storage-classify" / label;

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

std::string ChunkNameOf(const char fill) {
	return std::string(64, fill) + ".chunk";
}

std::vector<char> PatternBytes(const std::uint32_t bytes, const int seed) {
	std::vector<char> body;

	for (std::uint32_t position = 0; position < bytes; ++position) {
		body.push_back(static_cast<char>((position * 31 + seed * 17 + 7) & 0xFF));
	}

	return body;
}

void WriteWhole(const std::filesystem::path& target, const std::vector<char>& body) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

libtorrent::file_storage MakeSingleChunkLayout(
	const std::string& torrentName,
	const std::string& chunkFileName,
	const std::int64_t bytes
) {
	libtorrent::file_storage layout;

	layout.set_piece_length(PIECE_BYTES);
	layout.add_file(torrentName + "/" + chunkFileName, bytes);

	const std::int64_t pieceCount = (layout.total_size() + PIECE_BYTES - 1) / PIECE_BYTES;
	layout.set_num_pieces(static_cast<int>(pieceCount));

	return layout;
}

struct CompletedRead {
	std::vector<char> bytes;
	libtorrent::storage_error failure;
	bool completed = false;
};

CompletedRead ReadBlock(
	InstalledFolderStorage& storage,
	libtorrent::io_context& context,
	const libtorrent::storage_index_t index,
	const int length
) {
	CompletedRead result;

	storage.async_read(
		index,
		libtorrent::peer_request{libtorrent::piece_index_t(0), 0, length},
		[&result, length](libtorrent::disk_buffer_holder buffer, const libtorrent::storage_error& failure) {
			result.failure = failure;

			if (!failure.ec && buffer.data() != nullptr) {
				result.bytes.assign(buffer.data(), buffer.data() + length);
			}

			result.completed = true;
		}
	);

	storage.submit_jobs();

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

	while (!result.completed && std::chrono::steady_clock::now() < deadline) {
		context.restart();
		context.poll();

		if (!result.completed) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	return result;
}

libtorrent::status_t CheckFiles(
	InstalledFolderStorage& storage,
	const libtorrent::storage_index_t index,
	const libtorrent::add_torrent_params* resumeData
) {
	libtorrent::status_t observed{};

	storage.async_check_files(
		index,
		resumeData,
		{},
		[&observed](const libtorrent::status_t status, const libtorrent::storage_error&) {
			observed = status;
		}
	);

	return observed;
}

libtorrent::error_code MissingContentCode() {
	return libtorrent::error_code(
		boost::system::errc::no_such_file_or_directory,
		libtorrent::generic_category()
	);
}

libtorrent::error_code BrokenStorageCode() {
	return libtorrent::error_code(
		boost::system::errc::io_error,
		libtorrent::generic_category()
	);
}
}

TEST_CASE("seed read ignores overlapping fetch destinations") {
	const TemporaryTree tree("seed-overlap");

	const std::string chunkFileName = ChunkNameOf('a');
	const std::filesystem::path installedFile = tree.Root() / "installed" / "payload.dat";
	const std::filesystem::path destinationFile = tree.Root() / "fetch" / "target.dat";

	const std::vector<char> installedBytes = PatternBytes(CHUNK_BYTES, 5);

	WriteWhole(installedFile, installedBytes);
	WriteWhole(destinationFile, std::vector<char>(CHUNK_BYTES, char{0}));

	libtorrent::io_context context;
	StorageFaults faults;
	SeedAttestations attestations;
	StorageBacklog backlog;
	OpenFileCache handles;
	ChunkLocator locator;

	REQUIRE(locator.RegisterFile(chunkFileName, installedFile, 0, CHUNK_BYTES));
	REQUIRE(locator.RegisterDestination(chunkFileName, destinationFile, 0, CHUNK_BYTES));

	InstalledFolderStorage storage(locator, context, faults, attestations, backlog, handles);

	const libtorrent::file_storage layout =
			MakeSingleChunkLayout("seed_mod", chunkFileName, CHUNK_BYTES);
	const libtorrent::renamed_files renames;
	const libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities;
	const std::string savePath = tree.Root().string();

	const libtorrent::storage_params parameters(
		layout,
		renames,
		savePath,
		libtorrent::string_view(),
		libtorrent::storage_mode_sparse,
		priorities,
		libtorrent::sha1_hash(),
		false,
		true
	);

	const libtorrent::storage_holder held = storage.new_torrent(parameters, std::shared_ptr<void>());

	const CompletedRead result = ReadBlock(storage, context, held, static_cast<int>(CHUNK_BYTES));

	REQUIRE(result.completed);
	REQUIRE_FALSE(result.failure.ec);
	REQUIRE(result.bytes == installedBytes);
	REQUIRE(faults.ReadFailures() == 0);
}

TEST_CASE("absent held chunk reports missing without a fault") {
	const TemporaryTree tree("fetch-absent");

	const std::string chunkFileName = ChunkNameOf('b');

	libtorrent::io_context context;
	StorageFaults faults;
	SeedAttestations attestations;
	StorageBacklog backlog;
	OpenFileCache handles;
	ChunkLocator locator;

	InstalledFolderStorage storage(locator, context, faults, attestations, backlog, handles);

	const libtorrent::file_storage layout =
			MakeSingleChunkLayout("fetch_mod", chunkFileName, CHUNK_BYTES);
	const libtorrent::renamed_files renames;
	const libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities;

	const std::filesystem::path staging = tree.Root() / "staging";

	std::error_code creating;
	std::filesystem::create_directories(staging, creating);

	const std::string savePath = staging.string();

	const libtorrent::storage_params parameters(
		layout,
		renames,
		savePath,
		libtorrent::string_view(),
		libtorrent::storage_mode_sparse,
		priorities,
		libtorrent::sha1_hash(),
		false,
		true
	);

	const libtorrent::storage_holder held = storage.new_torrent(parameters, std::shared_ptr<void>());

	const CompletedRead result = ReadBlock(storage, context, held, static_cast<int>(CHUNK_BYTES));

	REQUIRE(result.completed);
	REQUIRE(result.failure.ec == MissingContentCode());
	REQUIRE(result.failure.file() == libtorrent::file_index_t(0));
	REQUIRE(faults.ReadFailures() == 0);
}

TEST_CASE("broken installed location counts a fault") {
	const TemporaryTree tree("seed-broken");

	const std::string chunkFileName = ChunkNameOf('c');
	const std::filesystem::path missingFile = tree.Root() / "missing" / "payload.dat";

	libtorrent::io_context context;
	StorageFaults faults;
	SeedAttestations attestations;
	StorageBacklog backlog;
	OpenFileCache handles;
	ChunkLocator locator;

	REQUIRE(locator.RegisterFile(chunkFileName, missingFile, 0, CHUNK_BYTES));

	InstalledFolderStorage storage(locator, context, faults, attestations, backlog, handles);

	const libtorrent::file_storage layout =
			MakeSingleChunkLayout("seed_mod", chunkFileName, CHUNK_BYTES);
	const libtorrent::renamed_files renames;
	const libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities;
	const std::string savePath = tree.Root().string();

	const libtorrent::storage_params parameters(
		layout,
		renames,
		savePath,
		libtorrent::string_view(),
		libtorrent::storage_mode_sparse,
		priorities,
		libtorrent::sha1_hash(),
		false,
		true
	);

	const libtorrent::storage_holder held = storage.new_torrent(parameters, std::shared_ptr<void>());

	const CompletedRead result = ReadBlock(storage, context, held, static_cast<int>(CHUNK_BYTES));

	REQUIRE(result.completed);
	REQUIRE(result.failure.ec == BrokenStorageCode());
	REQUIRE(faults.ReadFailures() == 1);
}

TEST_CASE("oversized chunk length registration is refused") {
	const TemporaryTree tree("oversized-length");

	const std::string chunkFileName = ChunkNameOf('d');
	const std::filesystem::path anyFile = tree.Root() / "payload.dat";
	const std::uint64_t oversized = std::uint64_t{1} << 32;

	ChunkLocator locator;

	REQUIRE_FALSE(locator.RegisterFile(chunkFileName, anyFile, 0, oversized));
	REQUIRE_FALSE(locator.Find(chunkFileName).has_value());
	REQUIRE(locator.Count() == 0);

	REQUIRE_FALSE(locator.RegisterDestination(chunkFileName, anyFile, 0, oversized));
	REQUIRE_FALSE(locator.HasDestination(chunkFileName));
	REQUIRE(locator.DestinationCount() == 0);
}

TEST_CASE("seed check skips hashing when resume attests every piece") {
	const TemporaryTree tree("seed-resume");

	const std::string chunkFileName = ChunkNameOf('e');
	const std::filesystem::path installedFile = tree.Root() / "installed" / "payload.dat";

	WriteWhole(installedFile, PatternBytes(CHUNK_BYTES, 9));

	libtorrent::io_context context;
	StorageFaults faults;
	SeedAttestations attestations;
	StorageBacklog backlog;
	OpenFileCache handles;
	ChunkLocator locator;

	REQUIRE(locator.RegisterFile(chunkFileName, installedFile, 0, CHUNK_BYTES));

	InstalledFolderStorage storage(locator, context, faults, attestations, backlog, handles);

	const libtorrent::file_storage layout =
			MakeSingleChunkLayout("seed_mod", chunkFileName, CHUNK_BYTES);
	const libtorrent::renamed_files renames;
	const libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities;
	const std::string savePath = tree.Root().string();

	const libtorrent::storage_params parameters(
		layout,
		renames,
		savePath,
		libtorrent::string_view(),
		libtorrent::storage_mode_sparse,
		priorities,
		libtorrent::sha1_hash(),
		false,
		true
	);

	const libtorrent::storage_holder held = storage.new_torrent(parameters, std::shared_ptr<void>());

	const libtorrent::status_t unattested = CheckFiles(storage, held, nullptr);

	REQUIRE(static_cast<bool>(unattested & libtorrent::disk_status::need_full_check));

	libtorrent::add_torrent_params partialResume;
	partialResume.have_pieces.resize(layout.num_pieces(), false);

	const libtorrent::status_t partial = CheckFiles(storage, held, &partialResume);

	REQUIRE(static_cast<bool>(partial & libtorrent::disk_status::need_full_check));

	libtorrent::add_torrent_params fullResume;
	fullResume.have_pieces.resize(layout.num_pieces(), true);

	const libtorrent::status_t attested = CheckFiles(storage, held, &fullResume);

	REQUIRE_FALSE(static_cast<bool>(attested & libtorrent::disk_status::need_full_check));
	REQUIRE(attested == libtorrent::status_t{});
}
