#pragma once

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "downloader/announce/AnnounceExchange.h"
#include "downloader/storage/ChunkLocator.h"
#include "downloader/storage/ModFolderStamp.h"
#include "downloader/storage/SeedAttestations.h"
#include "downloader/storage/OpenFileCache.h"
#include "downloader/storage/StorageBacklog.h"
#include "downloader/storage/SeedStampStore.h"
#include "downloader/storage/StorageFaults.h"
#include "downloader/torrent/build/TorrentCache.h"
#include "downloader/transfer/FetchState.h"
#include "downloader/transfer/PeerDialer.h"
#include "downloader/transfer/PresentHashSet.h"
#include "downloader/transfer/SeedRoster.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace libtorrent {
struct session;
struct torrent_handle;
}

namespace wgrd::downloader {
class TorrentSession final
		: public domain::ISwarmService,
		  public domain::ISeedingService,
		  public domain::IChunkFetcher,
		  public domain::IAnnounceGossip {
public:
	static constexpr std::string_view PUBLIC_INTERFACES = "0.0.0.0:6881,[::]:6881";
	static constexpr int PORT_RETRY_LIMIT = PeerDialer::PORT_RETRY_LIMIT;
	static constexpr std::uint16_t NEIGHBOUR_BASE_PORT = PeerDialer::NEIGHBOUR_BASE_PORT;
	static constexpr std::chrono::seconds NEIGHBOUR_DIAL_INTERVAL{5};
	static constexpr std::chrono::milliseconds STATUS_POLL_INTERVAL{200};
	static constexpr int LOCAL_ANNOUNCE_SECONDS = 30;
	static constexpr std::string_view TORRENT_CACHE_FOLDER = ".wgrdmm/torrents";
	static constexpr std::chrono::milliseconds WRITE_DRAIN_TIMEOUT{30000};
	static constexpr int CONTROL_CHANNEL_RESERVE_BYTES_PER_SECOND = 512 * 1024;
	static constexpr int BULK_LINK_BUDGET_BYTES_PER_SECOND = 8 * 1024 * 1024;
	static constexpr int BULK_RATE_CEILING_BYTES_PER_SECOND =
			BULK_LINK_BUDGET_BYTES_PER_SECOND - CONTROL_CHANNEL_RESERVE_BYTES_PER_SECOND;

	explicit TorrentSession(
		std::filesystem::path savePath,
		std::string listenInterfaces = std::string(PUBLIC_INTERFACES),
		bool discovery = true
	);

	void UseTorrentCache(std::filesystem::path folder);

	void StartGossip(
		domain::IAnnounceCatalogue& catalogue,
		domain::IAnnounceReceiver& receiver,
		const std::filesystem::path& dataDirectory
	);

	[[nodiscard]] const domain::GossipStatus& Gossip() const override;

	[[nodiscard]] bool AddGossipPeer(std::string_view address, std::uint16_t port) override;

	void ConnectLocalPeer(std::uint16_t port);

	[[nodiscard]] std::uint16_t ListenPort() const;

	TorrentSession(const TorrentSession&) = delete;

	TorrentSession& operator=(const TorrentSession&) = delete;

	~TorrentSession() override;

	[[nodiscard]] const domain::SwarmStatus& Status() const override;

	void Poll() override;

	[[nodiscard]] bool Enabled() const override;

	void SetEnabled(bool enabled) override;

	[[nodiscard]] std::expected<domain::SeedEntry, domain::SeedError> Announce(
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		const std::filesystem::path& sealedManifestPath
	) override;

	void AttestContent(
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		const std::filesystem::path& sealedManifestPath
	) override;

	bool StopSeeding(std::string_view identifier) override;

	[[nodiscard]] const std::vector<domain::SeedEntry>& Entries() const override;

	[[nodiscard]] std::uint64_t UploadedBytes() const override;

	[[nodiscard]] std::expected<void, domain::FetchError> Begin(
		std::string identifier,
		const domain::ChunkDigest& infoHash,
		const std::filesystem::path& stagingFolder,
		const std::vector<std::string>& wantedFiles,
		const std::vector<domain::ChunkDestination>& destinations,
		bool verifyExisting
	) override;

	[[nodiscard]] domain::FetchStatus Fetch() const override;

	void Cancel() override;

	[[nodiscard]] std::size_t RegisteredChunkFiles() const;

	[[nodiscard]] std::size_t RegisteredDestinations() const;

	[[nodiscard]] int SeededUploadLimit(std::string_view identifier) const;

	[[nodiscard]] int SeededDownloadLimit(std::string_view identifier) const;

	[[nodiscard]] int FetchUploadLimit() const;

	[[nodiscard]] int FetchDownloadLimit() const;

	[[nodiscard]] int ControlUploadLimit() const;

	[[nodiscard]] int ControlDownloadLimit() const;

private:
	static void CapBulkTransfer_(libtorrent::torrent_handle& handle);

	void DialLoopbackNeighbours_();

	void DialNeighbours_(libtorrent::torrent_handle& handle);

	void AccumulateRates_(int downloadRate, int uploadRate);

	void RefreshEntries_();

	ChunkLocator _locator;
	StorageFaults _faults;
	StorageBacklog _backlog;
	OpenFileCache _handles;
	SeedAttestations _attestations;
	SeedStampStore _stamps;
	TorrentCache _torrents;
	std::unique_ptr<AnnounceExchange> _exchange;
	std::unique_ptr<libtorrent::session> _session;
	std::unique_ptr<libtorrent::torrent_handle> _control;
	std::filesystem::path _savePath;
	domain::SwarmStatus _status;
	domain::GossipStatus _gossip;
	PeerDialer _dialer;
	std::chrono::steady_clock::time_point _lastNeighbourDial;
	std::chrono::steady_clock::time_point _lastStatusPoll;
	std::int64_t _pendingDownloadRate = 0;
	std::int64_t _pendingUploadRate = 0;
	std::uint32_t _controlPeers = 0;
	std::uint32_t _controlState = 0;
	std::string _lastPeerError;
	std::string _lastTransferFailure;
	SeedRoster _roster;
	mutable std::vector<domain::SeedEntry> _entriesView;
	bool _enabled;
	FetchState _fetchState;
	PresentHashSet _present;
};
}
