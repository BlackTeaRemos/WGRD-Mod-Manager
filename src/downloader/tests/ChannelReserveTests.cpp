#include "downloader/transfer/TorrentSession.h"

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/types/content/ChunkFileNaming.h"
#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::AnnounceRejection;
using wgrd::domain::AnnounceSummary;
using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFileNaming;
using wgrd::domain::IAnnounceCatalogue;
using wgrd::domain::IAnnounceReceiver;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::SignedAnnounce;
using wgrd::downloader::TorrentSession;

namespace {
constexpr std::string_view LOOPBACK = "127.0.0.1:0";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-reserve" / label;

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

class EmptyCatalogue final : public IAnnounceCatalogue {
public:
	EmptyCatalogue() = default;

	~EmptyCatalogue() override = default;

	[[nodiscard]] std::vector<AnnounceSummary> Summaries() const override {
		return {};
	}

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Record(
		const PublisherFingerprint&,
		std::string_view
	) const override {
		return std::nullopt;
	}

	[[nodiscard]] bool WouldAccept(
		const PublisherFingerprint&,
		std::string_view,
		std::uint64_t
	) const override {
		return false;
	}
};

class DiscardingReceiver final : public IAnnounceReceiver {
public:
	DiscardingReceiver() = default;

	~DiscardingReceiver() override = default;

	[[nodiscard]] std::expected<SignedAnnounce, AnnounceRejection> Accept(
		std::span<const std::uint8_t>
	) override {
		return SignedAnnounce{};
	}
};

ChunkDigest DigestFor(const std::size_t index) {
	const auto digest = ChunkDigest::FromHex(std::format("{:064x}", index + 1));
	REQUIRE(digest.has_value());
	return *digest;
}

ModManifest BuildManifest(const std::vector<std::uint32_t>& lengths) {
	std::vector<ManifestChunk> chunks;
	std::uint64_t offset = 0;

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		chunks.push_back(ManifestChunk{DigestFor(index), offset, lengths[index]});
		offset += lengths[index];
	}

	ManifestFile file{"packs/ZZ_Win.dat", offset, std::move(chunks)};

	return ModManifest(PublisherFingerprint{}, "angel_maps", 1, {file});
}

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 29 + 7) & 0xFF));
	}
}

void WriteSealed(const std::filesystem::path& target) {
	const std::vector<std::uint8_t> sealed(512, 0x5A);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));
}

std::uint16_t AwaitPort(TorrentSession& session) {
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

TEST_CASE("bulk runs unlimited until a budget is set") {
	const TemporaryTree tree("budget-default");

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);

	REQUIRE(session.LinkBudget() == 0);
	REQUIRE(session.BulkRateCeiling() == TorrentSession::UNLIMITED_RATE);
}

TEST_CASE("a budget leaves the control reserve to the control channel") {
	const TemporaryTree tree("budget-reserve");

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);

	constexpr std::int64_t BUDGET = 8 * 1024 * 1024;

	session.SetLinkBudget(BUDGET);

	REQUIRE(session.LinkBudget() == BUDGET);
	REQUIRE(session.BulkRateCeiling()
	        + TorrentSession::CONTROL_CHANNEL_RESERVE_BYTES_PER_SECOND
	        == BUDGET
	);
}

TEST_CASE("a tiny budget still leaves bulk able to move") {
	const TemporaryTree tree("budget-tiny");

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);

	session.SetLinkBudget(TorrentSession::CONTROL_CHANNEL_RESERVE_BYTES_PER_SECOND);

	REQUIRE(session.BulkRateCeiling() == TorrentSession::MINIMUM_BULK_RATE_BYTES_PER_SECOND);
}

TEST_CASE("seeded torrents follow the budget") {
	const TemporaryTree tree("seed-ceiling");

	const ModManifest manifest = BuildManifest({20000, 50000});

	const std::filesystem::path modFolder = tree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	const std::filesystem::path sealedPath = tree.Root() / "manifest.wgrdm";
	WriteSealed(sealedPath);

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(session) != 0);

	REQUIRE(session.Announce(manifest, modFolder, sealedPath).has_value());

	REQUIRE(session.SeededUploadLimit(manifest.Identifier()) == TorrentSession::UNLIMITED_RATE);
	REQUIRE(session.SeededDownloadLimit(manifest.Identifier()) == TorrentSession::UNLIMITED_RATE);

	session.SetLinkBudget(8 * 1024 * 1024);

	REQUIRE(session.SeededUploadLimit(manifest.Identifier()) == session.BulkRateCeiling());
	REQUIRE(session.SeededDownloadLimit(manifest.Identifier()) == session.BulkRateCeiling());
}

TEST_CASE("fetch torrents carry the bulk ceiling") {
	const TemporaryTree tree("fetch-ceiling");

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(session) != 0);

	const std::vector<std::string> wanted{ChunkFileNaming::FileNameFor(DigestFor(0))};

	REQUIRE(session.Begin(
			"test/angel_maps",
			DigestFor(99),
			tree.Root() / "staging",
			wanted,
			{},
			false
		).has_value()
	);

	REQUIRE(session.FetchUploadLimit() == TorrentSession::UNLIMITED_RATE);
	REQUIRE(session.FetchDownloadLimit() == TorrentSession::UNLIMITED_RATE);

	session.SetLinkBudget(8 * 1024 * 1024);

	REQUIRE(session.FetchUploadLimit() == session.BulkRateCeiling());
	REQUIRE(session.FetchDownloadLimit() == session.BulkRateCeiling());

	session.Cancel();
}

TEST_CASE("control swarm is uncapped") {
	const TemporaryTree tree("control-uncapped");

	EmptyCatalogue catalogue;
	DiscardingReceiver receiver;

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(session) != 0);

	session.StartGossip(catalogue, receiver, tree.Root() / "control");

	REQUIRE(session.Gossip().running);
	REQUIRE(session.ControlUploadLimit() <= 0);
	REQUIRE(session.ControlDownloadLimit() <= 0);
}
