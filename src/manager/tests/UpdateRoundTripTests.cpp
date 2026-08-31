#include "manager/hash/Blake3Hasher.h"
#include "manager/install/ContentInstaller.h"
#include "manager/install/ManifestDiffer.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/payload/PayloadPathPolicy.h"

#include "domain/interfaces/content/IChunkSource.h"
#include "domain/interfaces/content/IContentChunker.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFetchError;
using wgrd::domain::ChunkSourceKind;
using wgrd::domain::ChunkSpan;
using wgrd::domain::IChunkSource;
using wgrd::domain::IContentChunker;
using wgrd::domain::InstallPlan;
using wgrd::domain::ModManifest;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::ContentInstaller;
using wgrd::manager::ManifestBuilder;
using wgrd::manager::ManifestDiffer;
using wgrd::manager::PayloadPathPolicy;

namespace {
constexpr std::size_t CHUNK_LENGTH = 1024;

class FixedSizeChunker final : public IContentChunker {
public:
	~FixedSizeChunker() override = default;

	[[nodiscard]] std::vector<ChunkSpan> Split(const std::span<const std::byte> data) const override {
		std::vector<ChunkSpan> spans;

		std::size_t offset = 0;
		while (offset < data.size()) {
			const std::size_t length = std::min(CHUNK_LENGTH, data.size() - offset);
			spans.push_back(ChunkSpan{offset, length});
			offset += length;
		}

		return spans;
	}
};

class PublisherChunkSource final : public IChunkSource {
public:
	PublisherChunkSource(const ModManifest& manifest, std::filesystem::path folder)
		: _folder(std::move(folder))
		, _served(0) {
		for (const auto& file : manifest.Files()) {
			for (const auto& chunk : file.chunks) {
				_index.try_emplace(chunk.digest.ToHex(), Location{file.path, chunk.offset, chunk.length});
			}
		}
	}

	~PublisherChunkSource() override = default;

	[[nodiscard]] std::expected<std::vector<std::byte>, ChunkFetchError> Fetch(
		const ChunkDigest& digest,
		const std::uint32_t length
	) override {
		const auto match = _index.find(digest.ToHex());
		if (match == _index.end()) {
			return std::unexpected(ChunkFetchError::Unavailable);
		}

		if (match->second.length != length) {
			return std::unexpected(ChunkFetchError::LengthMismatch);
		}

		std::ifstream input(_folder / match->second.path, std::ios::binary);
		if (!input) {
			return std::unexpected(ChunkFetchError::Unavailable);
		}

		input.seekg(static_cast<std::streamoff>(match->second.offset));

		std::vector<std::byte> bytes(length);
		input.read(reinterpret_cast<char*>(bytes.data()), length);
		if (input.gcount() != static_cast<std::streamsize>(length)) {
			return std::unexpected(ChunkFetchError::LengthMismatch);
		}

		++_served;

		return bytes;
	}

	[[nodiscard]] std::size_t Served() const {
		return _served;
	}

private:
	struct Location {
		std::string path;
		std::uint64_t offset;
		std::uint32_t length;
	};

	std::filesystem::path _folder;
	std::map<std::string, Location> _index;
	std::size_t _served;
};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-update" / label;

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

void WriteBytes(const std::filesystem::path& target, const std::vector<std::uint8_t>& bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);
	const std::istreambuf_iterator<char> first(input);
	constexpr std::istreambuf_iterator<char> last;
	const std::string raw(first, last);

	std::vector<std::uint8_t> bytes;
	bytes.reserve(raw.size());
	for (const char character : raw) {
		bytes.push_back(static_cast<std::uint8_t>(character));
	}
	return bytes;
}

std::vector<std::uint8_t> MakePattern(const std::size_t length, const std::uint8_t seed) {
	std::vector<std::uint8_t> bytes(length);
	for (std::size_t position = 0; position < length; ++position) {
		bytes[position] = static_cast<std::uint8_t>((position * 31 + seed) & 0xFF);
	}
	return bytes;
}

wgrd::domain::PublisherFingerprint MakeFingerprint() {
	const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
	REQUIRE(fingerprint.has_value());
	return *fingerprint;
}
}

TEST_CASE("update reuses held chunks and fetches only what changed") {
	const TemporaryTree publisher("publisher");
	const TemporaryTree consumer("consumer");

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;
	const ContentInstaller installer(hasher);

	const std::vector<std::uint8_t> firstVersion = MakePattern(64 * CHUNK_LENGTH, 3);
	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", firstVersion);
	WriteBytes(publisher.Root() / "mod.json", MakePattern(200, 9));

	const auto heldManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 1);
	REQUIRE(heldManifest.has_value());
	REQUIRE(heldManifest->Files()[1].chunks.size() == 64);

	PublisherChunkSource firstSource(*heldManifest, publisher.Root());

	const InstallPlan firstPlan = differ.Diff(ModManifest(), *heldManifest);
	REQUIRE(firstPlan.RemoteChunkCount() == heldManifest->ChunkCount());
	REQUIRE(firstPlan.HeldBytes() == 0);

	const auto firstReport = installer.Apply(firstPlan, consumer.Root(), firstSource);
	REQUIRE(firstReport.has_value());
	REQUIRE(firstReport->filesWritten == 2);

	REQUIRE(ReadBytes(consumer.Root() / "packs" / "ZZ_Win.dat") == firstVersion);

	std::vector<std::uint8_t> secondVersion = firstVersion;
	for (std::size_t position = 0; position < 32; ++position) {
		secondVersion[10 * CHUNK_LENGTH + position] ^= 0xFF;
	}
	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", secondVersion);

	const auto targetManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 2);
	REQUIRE(targetManifest.has_value());

	const InstallPlan updatePlan = differ.Diff(*heldManifest, *targetManifest);

	REQUIRE(updatePlan.Files().size() == 1);
	REQUIRE(updatePlan.Files()[0].path == "packs/ZZ_Win.dat");
	REQUIRE(updatePlan.RemoteChunkCount() == 1);
	REQUIRE(updatePlan.RemoteBytes() == CHUNK_LENGTH);
	REQUIRE(updatePlan.HeldBytes() == 63 * CHUNK_LENGTH);

	PublisherChunkSource secondSource(*targetManifest, publisher.Root());
	const auto updateReport = installer.Apply(updatePlan, consumer.Root(), secondSource);

	REQUIRE(updateReport.has_value());
	REQUIRE(secondSource.Served() == 1);
	REQUIRE(updateReport->remoteBytes == CHUNK_LENGTH);

	REQUIRE(ReadBytes(consumer.Root() / "packs" / "ZZ_Win.dat") == secondVersion);
	REQUIRE(ReadBytes(consumer.Root() / "mod.json") == MakePattern(200, 9));
}

TEST_CASE("seeded update rewrites only the chunks that moved or changed") {
	const TemporaryTree publisher("seeded-publisher");
	const TemporaryTree consumer("seeded-consumer");

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;
	const ContentInstaller installer(hasher);

	const std::vector<std::uint8_t> firstVersion = MakePattern(64 * CHUNK_LENGTH, 3);
	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", firstVersion);
	WriteBytes(publisher.Root() / "mod.json", MakePattern(200, 9));

	const auto heldManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 1);
	REQUIRE(heldManifest.has_value());

	PublisherChunkSource firstSource(*heldManifest, publisher.Root());
	const auto firstReport = installer.Apply(
		differ.Diff(ModManifest(), *heldManifest),
		consumer.Root(),
		firstSource
	);

	REQUIRE(firstReport.has_value());

	std::vector<std::uint8_t> secondVersion = firstVersion;
	for (std::size_t position = 0; position < 32; ++position) {
		secondVersion[10 * CHUNK_LENGTH + position] ^= 0xFF;
	}

	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", secondVersion);

	const auto targetManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 2);
	REQUIRE(targetManifest.has_value());

	const InstallPlan updatePlan = differ.Diff(*heldManifest, *targetManifest);

	for (const wgrd::domain::FilePlan& file : updatePlan.Files()) {
		const std::filesystem::path target = consumer.Root() / file.path;
		const std::filesystem::path staged =
				std::filesystem::path(target.string() + std::string(ContentInstaller::STAGING_SUFFIX));

		std::error_code failure;
		std::filesystem::create_directories(staged.parent_path(), failure);

		if (std::filesystem::is_regular_file(target, failure)) {
			std::filesystem::copy_file(
				target,
				staged,
				std::filesystem::copy_options::overwrite_existing,
				failure
			);
		} else {
			std::ofstream created(staged, std::ios::binary | std::ios::trunc);
		}

		failure.clear();
		std::filesystem::resize_file(staged, static_cast<std::uintmax_t>(file.size), failure);
		REQUIRE_FALSE(failure);
	}

	PublisherChunkSource secondSource(*targetManifest, publisher.Root());
	const auto updateReport = installer.ApplyPlaced(updatePlan, consumer.Root(), secondSource);

	REQUIRE(updateReport.has_value());
	REQUIRE(secondSource.Served() == 0);
	REQUIRE(updateReport->heldBytes == 63 * CHUNK_LENGTH);
	REQUIRE(updateReport->filesWritten == 1);
}

TEST_CASE("seeded update grows a file and fetches only the appended chunks") {
	const TemporaryTree publisher("grow-publisher");
	const TemporaryTree consumer("grow-consumer");

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;
	const ContentInstaller installer(hasher);

	const std::vector<std::uint8_t> firstVersion = MakePattern(8 * CHUNK_LENGTH, 3);
	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", firstVersion);
	WriteBytes(publisher.Root() / "mod.json", MakePattern(200, 9));

	const auto heldManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 1);
	REQUIRE(heldManifest.has_value());

	PublisherChunkSource firstSource(*heldManifest, publisher.Root());
	REQUIRE(installer.Apply(
			differ.Diff(ModManifest(), *heldManifest),
			consumer.Root(),
			firstSource
		).has_value()
	);

	std::vector<std::uint8_t> grown = firstVersion;
	const std::vector<std::uint8_t> appended = MakePattern(3 * CHUNK_LENGTH, 77);
	grown.insert(grown.end(), appended.begin(), appended.end());

	WriteBytes(publisher.Root() / "packs" / "ZZ_Win.dat", grown);

	const auto targetManifest = builder.Build(publisher.Root(), MakeFingerprint(), "angel_maps", 2);
	REQUIRE(targetManifest.has_value());

	const InstallPlan updatePlan = differ.Diff(*heldManifest, *targetManifest);

	REQUIRE(updatePlan.RemoteChunkCount() == 3);

	for (const wgrd::domain::FilePlan& file : updatePlan.Files()) {
		const std::filesystem::path target = consumer.Root() / file.path;
		const std::filesystem::path staged =
				std::filesystem::path(target.string() + std::string(ContentInstaller::STAGING_SUFFIX));

		std::error_code failure;
		std::filesystem::create_directories(staged.parent_path(), failure);

		if (std::filesystem::is_regular_file(target, failure)) {
			std::filesystem::copy_file(
				target,
				staged,
				std::filesystem::copy_options::overwrite_existing,
				failure
			);
		} else {
			std::ofstream created(staged, std::ios::binary | std::ios::trunc);
		}

		failure.clear();
		std::filesystem::resize_file(staged, static_cast<std::uintmax_t>(file.size), failure);
		REQUIRE_FALSE(failure);
	}

	PublisherChunkSource secondSource(*targetManifest, publisher.Root());
	const auto updateReport = installer.Apply(updatePlan, consumer.Root(), secondSource);

	REQUIRE(updateReport.has_value());
	REQUIRE(secondSource.Served() == 3);
	REQUIRE(ReadBytes(consumer.Root() / "packs" / "ZZ_Win.dat") == grown);
}

TEST_CASE("install rejects a chunk whose bytes do not match its digest") {
	const TemporaryTree publisher("corrupt-publisher");
	const TemporaryTree consumer("corrupt-consumer");

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;
	const ContentInstaller installer(hasher);

	WriteBytes(publisher.Root() / "payload.dat", MakePattern(2 * CHUNK_LENGTH, 5));

	const auto manifest = builder.Build(publisher.Root(), MakeFingerprint(), "mod", 1);
	REQUIRE(manifest.has_value());

	class LyingChunkSource final : public IChunkSource {
	public:
		~LyingChunkSource() override = default;

		[[nodiscard]] std::expected<std::vector<std::byte>, ChunkFetchError> Fetch(
			const ChunkDigest&,
			const std::uint32_t length
		) override {
			return std::vector<std::byte>(length, std::byte{0x00});
		}
	};

	LyingChunkSource source;
	const InstallPlan plan = differ.Diff(ModManifest(), *manifest);

	const auto report = installer.Apply(plan, consumer.Root(), source);

	REQUIRE_FALSE(report.has_value());
	REQUIRE(report.error() == wgrd::manager::InstallError::RemoteChunkCorrupt);
	REQUIRE_FALSE(std::filesystem::exists(consumer.Root() / "payload.dat"));
}

TEST_CASE("diff reports files the target dropped") {
	const TemporaryTree publisher("removal");

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;

	WriteBytes(publisher.Root() / "keep.dat", MakePattern(100, 1));
	WriteBytes(publisher.Root() / "drop.dat", MakePattern(100, 2));

	const auto held = builder.Build(publisher.Root(), MakeFingerprint(), "mod", 1);
	REQUIRE(held.has_value());

	std::error_code failure;
	std::filesystem::remove(publisher.Root() / "drop.dat", failure);

	const auto target = builder.Build(publisher.Root(), MakeFingerprint(), "mod", 2);
	REQUIRE(target.has_value());

	const InstallPlan plan = differ.Diff(*held, *target);

	REQUIRE(plan.Removals().size() == 1);
	REQUIRE(plan.Removals()[0] == "drop.dat");
}
