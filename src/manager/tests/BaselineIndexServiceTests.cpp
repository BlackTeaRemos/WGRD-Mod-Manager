#include "domain/interfaces/content/IContentChunker.h"
#include "domain/rules/BaselinePackLayout.h"
#include "manager/baseline/BaselineIndexService.h"
#include "manager/baseline/records/BaselineRecordStore.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/ContentInstaller.h"
#include "manager/install/ManifestDiffer.h"
#include "manager/install/InstallPlanner.h"
#include "manager/install/FetchPlanner.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/payload/PayloadPathPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::BaselinePackLayout;
using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkSpan;
using wgrd::domain::IChunkSource;
using wgrd::domain::IContentChunker;
using wgrd::domain::InstallPlan;
using wgrd::domain::ModManifest;
using wgrd::manager::BaselineIndexService;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::ContentInstaller;
using wgrd::manager::ManifestBuilder;
using wgrd::manager::ManifestDiffer;
using wgrd::manager::PayloadPathPolicy;

namespace {
constexpr std::size_t CHUNK_LENGTH = 4096;
constexpr std::chrono::seconds AWAIT_LIMIT{20};

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

class RefusingChunkSource final : public IChunkSource {
public:
	~RefusingChunkSource() override = default;

	[[nodiscard]] std::expected<std::vector<std::byte>, wgrd::domain::ChunkFetchError> Fetch(
		const ChunkDigest&,
		std::uint32_t
	) override {
		++_requests;
		return std::unexpected(wgrd::domain::ChunkFetchError::Unavailable);
	}

	[[nodiscard]] std::size_t Requests() const {
		return _requests;
	}

private:
	std::size_t _requests = 0;
};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-baseline-index" / label;

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

std::vector<std::uint8_t> MakePattern(const std::size_t length, const std::uint8_t seed) {
	std::vector<std::uint8_t> bytes(length);
	for (std::size_t position = 0; position < length; ++position) {
		bytes[position] = static_cast<std::uint8_t>((position * 31 + (position / CHUNK_LENGTH) * 17 + seed) & 0xFF);
	}
	return bytes;
}

void WriteBytes(const std::filesystem::path& target, const std::vector<std::uint8_t>& bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);
	return std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

wgrd::domain::PublisherFingerprint MakeFingerprint() {
	const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
	REQUIRE(fingerprint.has_value());
	return *fingerprint;
}

bool AwaitComplete(const BaselineIndexService& service) {
	const auto deadline = std::chrono::steady_clock::now() + AWAIT_LIMIT;

	while (std::chrono::steady_clock::now() < deadline) {
		if (service.Status().Complete()) {
			return true;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	return false;
}
}

TEST_CASE("baseline indexer catalogues every pack under the game root") {
	const TemporaryTree game("catalogue");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;

	const std::filesystem::path packRoot = BaselinePackLayout::PackRoot(game.Root());
	const std::vector<std::uint8_t> pack = MakePattern(3 * CHUNK_LENGTH, 5);
	WriteBytes(packRoot / "48574" / "ZZ_Win.dat", pack);
	WriteBytes(packRoot / "48574" / "readme.txt", MakePattern(100, 1));

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
	service.Start();

	REQUIRE(AwaitComplete(service));
	REQUIRE(service.Status().totalFiles == 1);
	REQUIRE(service.Status().indexedBytes == pack.size());

	const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(pack.data()), pack.size());
	const auto located = service.Catalogue().Locate(hasher.Hash(bytes.subspan(CHUNK_LENGTH, CHUNK_LENGTH)));

	REQUIRE(located.has_value());
	REQUIRE(located->file == packRoot / "48574" / "ZZ_Win.dat");
	REQUIRE(located->offset == CHUNK_LENGTH);
	REQUIRE(located->length == CHUNK_LENGTH);
}

TEST_CASE("baseline indexer awaits only the counterparts it is asked for") {
	const TemporaryTree game("await");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;

	const std::filesystem::path packRoot = BaselinePackLayout::PackRoot(game.Root());
	WriteBytes(packRoot / "48574" / "ZZ_Win.dat", MakePattern(2 * CHUNK_LENGTH, 3));
	WriteBytes(packRoot / "48574" / "ZZ_1.dat", MakePattern(2 * CHUNK_LENGTH, 4));

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
	service.Start();

	const std::vector<std::string> wanted{"48574\\zz_1.dat"};
	REQUIRE(service.AwaitCounterparts(wanted, []() {
			return false;
		}
	));

	REQUIRE(AwaitComplete(service));
	REQUIRE(service.Status().indexedFiles == 2);
}

TEST_CASE("baseline indexer abandons a wait when told to") {
	const TemporaryTree game("abandon");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);

	const std::vector<std::string> wanted{"48574/ZZ_1.dat"};
	REQUIRE_FALSE(service.AwaitCounterparts(wanted, []() {
			return true;
		}
	));
}

TEST_CASE("baseline indexer restores records and drops stale ones") {
	const TemporaryTree game("restore");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;

	const std::filesystem::path packRoot = BaselinePackLayout::PackRoot(game.Root());
	const std::vector<std::uint8_t> stable = MakePattern(2 * CHUNK_LENGTH, 3);
	const std::vector<std::uint8_t> before = MakePattern(2 * CHUNK_LENGTH, 4);
	WriteBytes(packRoot / "48574" / "ZZ_Win.dat", stable);
	WriteBytes(packRoot / "48574" / "ZZ_1.dat", before);

	{
		BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
		service.Start();
		REQUIRE(AwaitComplete(service));
	}

	const wgrd::manager::BaselineRecordStore store(game.Root() / "records", hasher);
	REQUIRE(store.LoadAll().size() == 2);

	const std::vector<std::uint8_t> after = MakePattern(3 * CHUNK_LENGTH, 9);
	WriteBytes(packRoot / "48574" / "ZZ_1.dat", after);

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
	service.Start();
	REQUIRE(AwaitComplete(service));

	const std::span<const std::byte> beforeBytes(reinterpret_cast<const std::byte*>(before.data()), before.size());
	const std::span<const std::byte> afterBytes(reinterpret_cast<const std::byte*>(after.data()), after.size());

	REQUIRE_FALSE(service.Catalogue().Locate(hasher.Hash(beforeBytes.subspan(0, CHUNK_LENGTH))).has_value());
	REQUIRE(service.Catalogue().Locate(hasher.Hash(afterBytes.subspan(0, CHUNK_LENGTH))).has_value());
	REQUIRE(service.Status().indexedBytes == stable.size() + after.size());

	service.Stop();
	REQUIRE(std::filesystem::remove(packRoot / "48574" / "ZZ_1.dat"));
	service.Start();
	REQUIRE(AwaitComplete(service));
	REQUIRE_FALSE(service.Catalogue().Locate(hasher.Hash(afterBytes.subspan(0, CHUNK_LENGTH))).has_value());
	REQUIRE(service.Status().totalFiles == 1);
}

TEST_CASE("cold install materialises entirely from baseline chunks") {
	const TemporaryTree game("reuse");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy policy;
	const ManifestBuilder builder(chunker, hasher, policy);
	const ManifestDiffer differ;
	const ContentInstaller installer(hasher);

	const std::filesystem::path packRoot = BaselinePackLayout::PackRoot(game.Root());
	const std::vector<std::uint8_t> vanilla = MakePattern(4 * CHUNK_LENGTH, 7);
	WriteBytes(packRoot / "48574" / "ZZ_Win.dat", vanilla);

	std::vector<std::uint8_t> modded(vanilla.begin(), vanilla.begin() + 2 * CHUNK_LENGTH);
	modded.insert(modded.end(), vanilla.begin() + 3 * CHUNK_LENGTH, vanilla.end());
	modded.insert(modded.end(), vanilla.begin() + 2 * CHUNK_LENGTH, vanilla.begin() + 3 * CHUNK_LENGTH);

	const std::filesystem::path publisher = game.Root() / "publisher";
	WriteBytes(publisher / "48574" / "ZZ_Win.dat", modded);

	const auto target = builder.Build(publisher, MakeFingerprint(), "mod", 1);
	REQUIRE(target.has_value());

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
	service.Start();
	REQUIRE(AwaitComplete(service));

	const std::filesystem::path modFolder = game.Root() / "mods" / "mod";
	const wgrd::manager::InstallPlanner planner(game.Root() / "mods", builder, differ, &service);
	const auto planned = planner.Plan(*target, []() {
		return false;
	}, [](std::string_view) {
	});
	REQUIRE(planned.has_value());
	InstallPlan plan = *planned;

	REQUIRE(plan.RemoteChunkCount() == 0);
	REQUIRE(plan.HeldBytes() == modded.size());

	const wgrd::manager::FetchPlanner fetchPlanner(hasher);
	const auto staged = fetchPlanner.Stage(plan, modFolder, ContentInstaller::STAGING_SUFFIX);
	REQUIRE(staged.has_value());
	REQUIRE(staged->wantedFiles.empty());

	std::optional<wgrd::manager::InstallError> expectedFailure;
	SECTION("unchanged source") {
	}
	SECTION("changed source rejected") {
		WriteBytes(packRoot / "48574" / "ZZ_Win.dat", MakePattern(vanilla.size(), 19));
		expectedFailure = wgrd::manager::InstallError::HeldChunkCorrupt;
	}
	SECTION("offset overflow rejected") {
		std::vector<wgrd::domain::FilePlan> files(plan.Files().begin(), plan.Files().end());
		files.front().placements.front().heldOffset = std::numeric_limits<std::uint64_t>::max();
		plan = InstallPlan(std::move(files), {});
		expectedFailure = wgrd::manager::InstallError::HeldChunkUnreadable;
	}

	RefusingChunkSource source;
	const auto report = installer.ApplyPlaced(plan, modFolder, source);

	if (expectedFailure.has_value()) {
		REQUIRE_FALSE(report.has_value());
		REQUIRE(report.error() == *expectedFailure);
		REQUIRE_FALSE(std::filesystem::exists(modFolder / "48574" / "ZZ_Win.dat"));
		return;
	}

	REQUIRE(report.has_value());
	REQUIRE(source.Requests() == 0);
	REQUIRE(report->heldBytes == modded.size());
	REQUIRE(ReadBytes(modFolder / "48574" / "ZZ_Win.dat") == modded);
	REQUIRE(ReadBytes(packRoot / "48574" / "ZZ_Win.dat") == vanilla);
}

TEST_CASE("unreadable baseline reports failure") {
	const TemporaryTree game("unreadable");
	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const std::filesystem::path file = BaselinePackLayout::PackRoot(game.Root()) / "pack.dat";
	WriteBytes(file, MakePattern(CHUNK_LENGTH, 7));

	std::ofstream writer(file, std::ios::binary | std::ios::app);
	REQUIRE(writer.is_open());

	BaselineIndexService service(game.Root(), game.Root() / "records", chunker, hasher);
	service.Start();
	REQUIRE(AwaitComplete(service));
	REQUIRE(service.Status().indexedFiles == 0);
	REQUIRE(service.Status().failedFiles == 1);
	REQUIRE(service.Status().indexedBytes == 0);
}
