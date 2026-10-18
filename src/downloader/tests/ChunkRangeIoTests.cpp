#include "downloader/storage/ChunkRangeIo.h"

#include <catch2/catch_test_macros.hpp>

#include <libtorrent/file_storage.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::downloader::ChunkLocator;
using wgrd::downloader::ChunkRangeIo;
using wgrd::downloader::OpenFileCache;
using wgrd::downloader::ReadOutcome;
using wgrd::downloader::StorageRole;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string& name)
		: _root(std::filesystem::temp_directory_path() / "wgrd-range" / name) {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
		std::filesystem::create_directories(_root, failure);
	}

	~TemporaryTree() {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}

	const std::filesystem::path& Root() const {
		return _root;
	}

private:
	std::filesystem::path _root;
};

libtorrent::file_storage Layout(const std::string& name, const std::int64_t length) {
	libtorrent::file_storage files;
	files.set_piece_length(16);
	files.add_file(name, length);
	files.set_num_pieces(1);
	return files;
}

void WriteBytes(const std::filesystem::path& target, const std::array<char, 16>& bytes) {
	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
}

TEST_CASE("T24 range preserves guards") {
	const TemporaryTree tree("guards");
	const std::filesystem::path source = tree.Root() / "source.bin";
	const std::array<char, 16> bytes = {'G', 'A', 'R', 'D', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'X', 'Y'};
	WriteBytes(source, bytes);

	ChunkLocator locator;
	REQUIRE(locator.RegisterFile("chunk", source, 0, bytes.size()));
	OpenFileCache handles;
	ChunkRangeIo io(locator, handles);
	const libtorrent::file_storage files = Layout("chunk", bytes.size());
	std::array<char, 4> target = {'?', '?', '?', '?'};
	libtorrent::file_index_t failedFile(0);

	REQUIRE(io.Read(files, tree.Root(), StorageRole::SeedRead, libtorrent::piece_index_t(0), 4, 4, target.data(), failedFile) == ReadOutcome::Success);
	REQUIRE(target == std::array<char, 4>{'0', '1', '2', '3'});
}

TEST_CASE("T24 range writes destinations") {
	const TemporaryTree tree("destinations");
	const std::filesystem::path first = tree.Root() / "first.bin";
	const std::filesystem::path second = tree.Root() / "second.bin";
	const std::array<char, 16> initial{};
	WriteBytes(first, initial);
	WriteBytes(second, initial);

	ChunkLocator locator;
	REQUIRE(locator.RegisterDestination("chunk", first, 2, 8));
	REQUIRE(locator.RegisterDestination("chunk", second, 4, 8));
	OpenFileCache handles;
	ChunkRangeIo io(locator, handles);
	const libtorrent::file_storage files = Layout("chunk", 8);
	const std::array<char, 8> source = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

	REQUIRE(io.Write(files, tree.Root(), StorageRole::FetchWrite, libtorrent::piece_index_t(0), 0, source.size(), source.data()));

	std::ifstream firstInput(first, std::ios::binary);
	std::ifstream secondInput(second, std::ios::binary);
	firstInput.seekg(2);
	secondInput.seekg(4);
	std::array<char, 8> firstRead{};
	std::array<char, 8> secondRead{};
	firstInput.read(firstRead.data(), firstRead.size());
	secondInput.read(secondRead.data(), secondRead.size());
	REQUIRE(firstRead == source);
	REQUIRE(secondRead == source);
}

TEST_CASE("T24 short backing refuses") {
	const TemporaryTree tree("short");
	const std::filesystem::path source = tree.Root() / "source.bin";
	const std::array<char, 4> bytes = {'a', 'b', 'c', 'd'};
	std::ofstream output(source, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), bytes.size());

	ChunkLocator locator;
	REQUIRE(locator.RegisterFile("chunk", source, 0, bytes.size()));
	OpenFileCache handles;
	ChunkRangeIo io(locator, handles);
	const libtorrent::file_storage files = Layout("chunk", 8);
	std::array<char, 8> target{};
	libtorrent::file_index_t failedFile(0);

	REQUIRE(io.Read(files, tree.Root(), StorageRole::SeedRead, libtorrent::piece_index_t(0), 0, target.size(), target.data(), failedFile) == ReadOutcome::Fault);
}
