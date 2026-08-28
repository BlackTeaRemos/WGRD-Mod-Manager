#pragma once

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "downloader/announce/AnnounceExchange.h"
#include "downloader/storage/ChunkLocator.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace libtorrent {
class session;
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
	static constexpr int PORT_RETRY_LIMIT = 20;
	static constexpr std::uint16_t NEIGHBOUR_BASE_PORT = 6881;
	static constexpr std::chrono::seconds NEIGHBOUR_DIAL_INTERVAL{5};
	static constexpr std::chrono::milliseconds STATUS_POLL_INTERVAL{200};

	explicit TorrentSession(
		std::filesystem::path savePath,
		std::string listenInterfaces = std::string(PUBLIC_INTERFACES),
		bool discovery = true
	);

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

	bool StopSeeding(std::string_view identifier) override;

	[[nodiscard]] const std::vector<domain::SeedEntry>& Entries() const override;

	[[nodiscard]] std::uint64_t UploadedBytes() const override;

	[[nodiscard]] std::expected<void, domain::FetchError> Begin(
		std::string identifier,
		const domain::ChunkDigest& infoHash,
		const std::filesystem::path& stagingFolder,
		const std::vector<std::string>& wantedFiles
	) override;

	[[nodiscard]] domain::FetchStatus Fetch() const override;

	void Cancel() override;

private:
	struct SeededTorrent {
		std::string identifier;
		std::shared_ptr<libtorrent::torrent_handle> handle;
		domain::ModManifest manifest;
	};

	[[nodiscard]] bool AlreadySeeding_(const std::string& identifier) const;

	void DialManualPeers_(libtorrent::torrent_handle& handle) const;

	void DialLoopbackNeighbours_();

	void DialNeighbours_(libtorrent::torrent_handle& handle);

	void AccumulateRates_(int downloadRate, int uploadRate);

	void RefreshEntries_();

	void ApplyFetchPriorities_();

	void RefreshFetch_();

	ChunkLocator _locator;
	std::unique_ptr<AnnounceExchange> _exchange;
	std::unique_ptr<libtorrent::session> _session;
	std::unique_ptr<libtorrent::torrent_handle> _control;
	std::filesystem::path _savePath;
	domain::SwarmStatus _status;
	domain::GossipStatus _gossip;
	std::vector<std::pair<std::string, std::uint16_t>> _manualPeers;
	std::chrono::steady_clock::time_point _lastNeighbourDial;
	std::chrono::steady_clock::time_point _lastStatusPoll;
	std::int64_t _pendingDownloadRate = 0;
	std::int64_t _pendingUploadRate = 0;
	std::uint32_t _controlPeers = 0;
	std::uint32_t _controlState = 0;
	std::uint64_t _neighbourDials;
	std::string _lastPeerError;
	std::vector<SeededTorrent> _seeded;
	std::vector<domain::SeedEntry> _entries;
	bool _enabled;
	std::unique_ptr<libtorrent::torrent_handle> _fetching;
	std::set<std::string> _wanted;
	bool _prioritised;
	domain::FetchStatus _fetch;
};
}
