#pragma once

#include "manager/service/InstallService.h"

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/types/content/ChunkFileNaming.h"
#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace wgrd::manager::tests {
constexpr std::size_t CHUNK_LENGTH = 2048;

class FixedSizeChunker final : public domain::IContentChunker {
public:
	~FixedSizeChunker() override = default;

	[[nodiscard]] std::vector<domain::ChunkSpan> Split(const std::span<const std::byte> data) const override {
		std::vector<domain::ChunkSpan> spans;

		std::size_t offset = 0;
		while (offset < data.size()) {
			const std::size_t length = std::min(CHUNK_LENGTH, data.size() - offset);
			spans.push_back(domain::ChunkSpan{offset, length});
			offset += length;
		}

		return spans;
	}
};

class StubTorrentBuilder final : public domain::IChunkSetTorrentBuilder {
public:
	~StubTorrentBuilder() override = default;

	[[nodiscard]] std::expected<domain::ChunkSetTorrentDescription, domain::ChunkSetTorrentError> Build(
		const domain::ModManifest& manifest,
		const std::filesystem::path&,
		std::span<const std::uint8_t>
	) const override {
		const auto infoHash = domain::ChunkDigest::FromHex(std::string(64, 'a'));
		REQUIRE(infoHash.has_value());

		return domain::ChunkSetTorrentDescription{
			{'d', 'e'}, *infoHash, manifest.TotalBytes(), manifest.ChunkCount()
		};
	}
};

class CopyingFetcher final : public domain::IChunkFetcher {
public:
	CopyingFetcher(
		const domain::ModManifest& manifest,
		std::filesystem::path sourceFolder,
		std::filesystem::path sealedManifestPath = {}
	)
		: _manifest(manifest)
		, _sourceFolder(std::move(sourceFolder))
		, _sealedManifestPath(std::move(sealedManifestPath))
		, _status() {}

	~CopyingFetcher() override = default;

	[[nodiscard]] std::expected<void, domain::FetchError> Begin(
		std::string identifier,
		const domain::ChunkDigest&,
		const std::filesystem::path& stagingFolder,
		const std::vector<std::string>& wantedFiles,
		const std::vector<domain::ChunkDestination>& destinations,
		bool
	) override {
		const std::filesystem::path target = stagingFolder / _manifest.TorrentName();

		std::error_code failure;
		std::filesystem::create_directories(target, failure);

		for (const std::string& wantedName : wantedFiles) {
			if (wantedName != domain::ChunkFileNaming::MANIFEST_FILE) {
				continue;
			}

			std::filesystem::copy_file(
				_sealedManifestPath,
				target / wantedName,
				std::filesystem::copy_options::overwrite_existing,
				failure
			);

			++_served;
		}

		for (const domain::ChunkDestination& destination : destinations) {
			for (const auto& file : _manifest.Files()) {
				for (const auto& chunk : file.chunks) {
					if (domain::ChunkFileNaming::FileNameFor(chunk.digest) != destination.chunkFileName) {
						continue;
					}

					std::ifstream input(_sourceFolder / file.path, std::ios::binary);
					input.seekg(static_cast<std::streamoff>(chunk.offset));

					std::vector<char> bytes(chunk.length);
					input.read(bytes.data(), chunk.length);

					std::fstream output(
						destination.file,
						std::ios::binary | std::ios::in | std::ios::out
					);

					output.seekp(static_cast<std::streamoff>(destination.offset));
					output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

					++_served;
				}
			}
		}

		_status.phase = domain::FetchPhase::Complete;
		_status.identifier = std::move(identifier);
		_status.stagingFolder = stagingFolder;

		return {};
	}

	[[nodiscard]] domain::FetchStatus Fetch() const override {
		return _status;
	}

	void Cancel() override {
		_status = domain::FetchStatus{};
	}

	[[nodiscard]] std::size_t Served() const {
		return _served;
	}

private:
	domain::ModManifest _manifest;
	std::filesystem::path _sourceFolder;
	std::filesystem::path _sealedManifestPath;
	domain::FetchStatus _status;
	std::size_t _served = 0;
};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-install" / label;

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

inline void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes, const std::uint8_t seed) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>(((position * 13) ^ (position >> 8) ^ seed) & 0xFF));
	}
}

inline std::vector<char> ReadAll(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);
	return std::vector<char>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

inline bool AwaitPhase(InstallService& service, const domain::InstallPhase phase, const std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	while (std::chrono::steady_clock::now() < deadline) {
		service.Poll();

		const domain::InstallPhase current = service.Progress().phase;
		if (current == phase) {
			return true;
		}

		if (current == domain::InstallPhase::Failed) {
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return false;
}
}
