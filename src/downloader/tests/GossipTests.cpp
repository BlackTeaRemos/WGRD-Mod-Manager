#include "downloader/announce/AnnounceWireCodec.h"
#include "downloader/transfer/TorrentSession.h"

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::AnnounceRejection;
using wgrd::domain::AnnounceSummary;
using wgrd::domain::IAnnounceCatalogue;
using wgrd::domain::IAnnounceReceiver;
using wgrd::domain::PublisherFingerprint;
using wgrd::domain::SignedAnnounce;
using wgrd::downloader::AnnounceWireCodec;
using wgrd::downloader::TorrentSession;

namespace {
constexpr std::string_view LOOPBACK = "127.0.0.1:0";
constexpr std::chrono::seconds BUDGET{45};
constexpr std::chrono::milliseconds POLL_INTERVAL{50};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-gossip" / label;

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

PublisherFingerprint MakeFingerprint(const std::uint8_t seed) {
	const std::array<std::uint8_t, 8> raw = {seed, 2, 3, 4, 5, 6, 7, 8};

	const auto fingerprint = PublisherFingerprint::FromBytes(raw);
	REQUIRE(fingerprint.has_value());

	return *fingerprint;
}

std::vector<std::uint8_t> MakeRecord(const std::uint8_t seed) {
	std::vector<std::uint8_t> record(AnnounceWireCodec::RECORD_BYTES);

	for (std::size_t index = 0; index < record.size(); ++index) {
		record[index] = static_cast<std::uint8_t>((index * 7 + seed) & 0xFF);
	}

	return record;
}

class MemoryCatalogue final : public IAnnounceCatalogue {
public:
	MemoryCatalogue() = default;

	~MemoryCatalogue() override = default;

	void Hold(const AnnounceSummary& summary, std::vector<std::uint8_t> record) {
		const std::scoped_lock lock(_guard);
		_summaries.push_back(summary);
		_records.push_back(std::move(record));
	}

	[[nodiscard]] std::vector<AnnounceSummary> Summaries() const override {
		const std::scoped_lock lock(_guard);
		return _summaries;
	}

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Record(
		const PublisherFingerprint& publisher,
		const std::string_view modName
	) const override {
		const std::scoped_lock lock(_guard);

		for (std::size_t index = 0; index < _summaries.size(); ++index) {
			if (_summaries[index].publisher == publisher && _summaries[index].modName == modName) {
				return _records[index];
			}
		}

		return std::nullopt;
	}

	[[nodiscard]] bool WouldAccept(
		const PublisherFingerprint& publisher,
		const std::string_view modName,
		const std::uint64_t version
	) const override {
		const std::scoped_lock lock(_guard);

		for (const AnnounceSummary& summary : _summaries) {
			if (summary.publisher == publisher && summary.modName == modName) {
				return version > summary.version;
			}
		}

		return true;
	}

private:
	mutable std::mutex _guard;
	std::vector<AnnounceSummary> _summaries;
	std::vector<std::vector<std::uint8_t>> _records;
};

class MemoryReceiver final : public IAnnounceReceiver {
public:
	MemoryReceiver() = default;

	~MemoryReceiver() override = default;

	[[nodiscard]] std::expected<SignedAnnounce, AnnounceRejection> Accept(
		std::span<const std::uint8_t> record
	) override {
		const std::scoped_lock lock(_guard);
		_accepted.emplace_back(record.begin(), record.end());

		return SignedAnnounce{};
	}

	[[nodiscard]] std::size_t Count() const {
		const std::scoped_lock lock(_guard);
		return _accepted.size();
	}

	[[nodiscard]] std::vector<std::uint8_t> First() const {
		const std::scoped_lock lock(_guard);
		return _accepted.empty() ? std::vector<std::uint8_t>() : _accepted.front();
	}

private:
	mutable std::mutex _guard;
	std::vector<std::vector<std::uint8_t>> _accepted;
};
}

TEST_CASE("two live sessions gossip over loopback", "[.live]") {
	const TemporaryTree seederTree("live-seeder");
	const TemporaryTree leecherTree("live-leecher");

	MemoryCatalogue seederCatalogue;
	MemoryReceiver seederReceiver;

	MemoryCatalogue leecherCatalogue;
	MemoryReceiver leecherReceiver;

	const AnnounceSummary summary{MakeFingerprint(0x31), "live_maps", 5};
	const std::vector<std::uint8_t> record = MakeRecord(0x77);

	seederCatalogue.Hold(summary, record);

	const std::string interfaces = std::string(TorrentSession::PUBLIC_INTERFACES);

	TorrentSession seeder(seederTree.Root(), interfaces, true);
	TorrentSession leecher(leecherTree.Root(), interfaces, true);

	seeder.StartGossip(seederCatalogue, seederReceiver, seederTree.Root() / "control");
	leecher.StartGossip(leecherCatalogue, leecherReceiver, leecherTree.Root() / "control");

	const auto deadline = std::chrono::steady_clock::now() + BUDGET;

	while (std::chrono::steady_clock::now() < deadline) {
		seeder.Poll();
		leecher.Poll();

		if (leecherReceiver.Count() > 0) {
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	UNSCOPED_INFO("seeder port " << seeder.ListenPort() << " leecher port " << leecher.ListenPort());
	UNSCOPED_INFO("seeder control peers " << seeder.Gossip().controlPeers);
	UNSCOPED_INFO("leecher control peers " << leecher.Gossip().controlPeers);
	UNSCOPED_INFO("leecher dials " << leecher.Gossip().neighbourDials);
	UNSCOPED_INFO("leecher gossip peers " << leecher.Gossip().peers);
	UNSCOPED_INFO("seeder gossip peers " << seeder.Gossip().peers);
	UNSCOPED_INFO("offers received " << leecher.Gossip().offersReceived);

	REQUIRE(leecherReceiver.Count() == 1);
}

TEST_CASE("control torrent reaches a seeding state", "[.live]") {
	const TemporaryTree tree("control-state");

	MemoryCatalogue catalogue;
	MemoryReceiver receiver;

	TorrentSession session(
		tree.Root(),
		std::string(TorrentSession::PUBLIC_INTERFACES),
		true
	);

	session.StartGossip(catalogue, receiver, tree.Root() / "control");

	const auto deadline = std::chrono::steady_clock::now() + BUDGET;

	while (std::chrono::steady_clock::now() < deadline) {
		session.Poll();

		if (session.Gossip().neighbourDials > 0) {
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	UNSCOPED_INFO("control valid " << session.Gossip().controlValid);
	UNSCOPED_INFO("control state " << session.Gossip().controlState);
	UNSCOPED_INFO("dials " << session.Gossip().neighbourDials);
	UNSCOPED_INFO("listen port " << session.ListenPort());

	REQUIRE(session.Gossip().controlValid);
	REQUIRE(session.Gossip().neighbourDials > 0);
	REQUIRE(session.Gossip().controlState == 5);
}

TEST_CASE("one process gossips with another copy of itself", "[.pair]") {
	std::size_t roleLength = 0;
	char roleBuffer[16] = {};
	getenv_s(&roleLength, roleBuffer, sizeof(roleBuffer), "WGRD_PAIR_ROLE");

	const std::string role(roleBuffer);
	const bool seeding = role == "seed";

	const TemporaryTree tree(seeding ? "pair-seed" : "pair-leech");

	MemoryCatalogue catalogue;
	MemoryReceiver receiver;

	const AnnounceSummary summary{MakeFingerprint(0x41), "pair_maps", 9};

	if (seeding) {
		catalogue.Hold(summary, MakeRecord(0x99));
	}

	TorrentSession session(
		tree.Root(),
		std::string(TorrentSession::PUBLIC_INTERFACES),
		true
	);

	session.StartGossip(catalogue, receiver, tree.Root() / "control");

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(40);

	while (std::chrono::steady_clock::now() < deadline) {
		session.Poll();

		if (!seeding && receiver.Count() > 0) {
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	const auto status = session.Gossip();

	WARN("role " << (seeding ? "seed" : "leech")
		<< " port " << session.ListenPort()
		<< " controlPeers " << status.controlPeers
		<< " gossipPeers " << status.peers
		<< " dials " << status.neighbourDials
		<< " offersIn " << status.offersReceived
		<< " recordsIn " << status.recordsReceived
		<< " accepted " << status.recordsAccepted
		<< " received " << receiver.Count()
		<< " lastPeerError [" << status.lastPeerError << "]"
	);
}

TEST_CASE("gossip finds a neighbour on loopback without dialling") {
	const TemporaryTree seederTree("auto-seeder");
	const TemporaryTree leecherTree("auto-leecher");

	MemoryCatalogue seederCatalogue;
	MemoryReceiver seederReceiver;

	MemoryCatalogue leecherCatalogue;
	MemoryReceiver leecherReceiver;

	const AnnounceSummary summary{MakeFingerprint(0x21), "auto_maps", 2};
	const std::vector<AnnounceSummary> holdings = {summary};
	const std::vector<std::uint8_t> record = MakeRecord(0x55);

	seederCatalogue.Hold(summary, record);

	const std::string interfaces = std::string(TorrentSession::PUBLIC_INTERFACES);

	TorrentSession seeder(seederTree.Root(), interfaces, false);
	TorrentSession leecher(leecherTree.Root(), interfaces, false);

	seeder.StartGossip(seederCatalogue, seederReceiver, seederTree.Root() / "control");
	leecher.StartGossip(leecherCatalogue, leecherReceiver, leecherTree.Root() / "control");

	REQUIRE(seeder.Gossip().running);
	REQUIRE(leecher.Gossip().running);

	REQUIRE(seeder.ListenPort() != leecher.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + BUDGET;

	while (std::chrono::steady_clock::now() < deadline) {
		seeder.Poll();
		leecher.Poll();

		if (leecherReceiver.Count() > 0) {
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	UNSCOPED_INFO("seeder control peers " << seeder.Gossip().controlPeers);
	UNSCOPED_INFO("leecher control peers " << leecher.Gossip().controlPeers);
	UNSCOPED_INFO("leecher gossip peers " << leecher.Gossip().peers);
	UNSCOPED_INFO("seeder port " << seeder.ListenPort());
	UNSCOPED_INFO("leecher port " << leecher.ListenPort());

	REQUIRE(leecher.Gossip().peers > 0);
	REQUIRE(leecherReceiver.Count() >= 1);
	REQUIRE(leecherReceiver.First() == record);
}

TEST_CASE("gossip carries a record between two sessions") {
	const TemporaryTree seederTree("seeder");
	const TemporaryTree leecherTree("leecher");

	MemoryCatalogue seederCatalogue;
	MemoryReceiver seederReceiver;

	MemoryCatalogue leecherCatalogue;
	MemoryReceiver leecherReceiver;

	const AnnounceSummary summary{MakeFingerprint(0x11), "angel_maps", 4};
	const std::vector<std::uint8_t> record = MakeRecord(0x33);

	seederCatalogue.Hold(summary, record);

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);

	seeder.StartGossip(seederCatalogue, seederReceiver, seederTree.Root() / "control");
	leecher.StartGossip(leecherCatalogue, leecherReceiver, leecherTree.Root() / "control");

	REQUIRE(seeder.Gossip().running);
	REQUIRE(leecher.Gossip().running);

	const std::uint16_t seederPort = seeder.ListenPort();
	REQUIRE(seederPort != 0);

	const auto deadline = std::chrono::steady_clock::now() + BUDGET;

	bool dialled = false;

	while (std::chrono::steady_clock::now() < deadline) {
		if (!dialled) {
			dialled = leecher.AddGossipPeer("127.0.0.1", seederPort);
		}

		seeder.Poll();
		leecher.Poll();

		if (leecherReceiver.Count() > 0) {
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	REQUIRE(dialled);
	REQUIRE(leecher.Gossip().peers > 0);
	REQUIRE(leecherReceiver.Count() >= 1);
	REQUIRE(leecherReceiver.First() == record);
}
