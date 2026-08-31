#pragma once

#include "downloader/transfer/TorrentSession.h"

#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace wgrd::downloader::tests {
constexpr std::string_view LOOPBACK = "127.0.0.1:0";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-fetch" / label;

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

inline domain::ChunkDigest DigestFor(const std::size_t index) {
	const auto digest = domain::ChunkDigest::FromHex(std::format("{:064x}", index + 1));
	REQUIRE(digest.has_value());
	return *digest;
}

inline domain::ModManifest BuildNamedManifest(
	const std::vector<std::uint32_t>& lengths,
	const std::string& modName
) {
	std::vector<domain::ManifestChunk> chunks;
	std::uint64_t offset = 0;

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		chunks.push_back(domain::ManifestChunk{DigestFor(index), offset, lengths[index]});
		offset += lengths[index];
	}

	domain::ManifestFile file{"packs/ZZ_Win.dat", offset, std::move(chunks)};

	return domain::ModManifest(domain::PublisherFingerprint{}, modName, 1, {file});
}

inline domain::ModManifest BuildManifest(const std::vector<std::uint32_t>& lengths) {
	return BuildNamedManifest(lengths, "angel_maps");
}

inline void WriteFilled(
	const std::filesystem::path& target,
	const std::uint64_t bytes,
	const char fill
) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(fill);
	}
}

inline std::vector<char> ReadWhole(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);

	return std::vector<char>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

inline void WriteSealed(const std::filesystem::path& target) {
	const std::vector<std::uint8_t> sealed(512, 0x5A);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));
}

inline void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 29 + 7) & 0xFF));
	}
}

inline std::uint16_t AwaitPort(TorrentSession& session) {
	for (int attempt = 0; attempt < 400; ++attempt) {
		session.Poll();

		const std::uint16_t port = session.ListenPort();
		if (port != 0) {
			return port;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	return 0;
}
}
