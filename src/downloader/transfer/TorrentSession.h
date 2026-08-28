#pragma once

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "downloader/storage/ChunkLocator.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace libtorrent {
class session;
struct torrent_handle;
}

namespace wgrd::downloader {

class TorrentSession final
    : public domain::ISwarmService,
      public domain::ISeedingService,
      public domain::IChunkFetcher {
public:
    static constexpr std::string_view PUBLIC_INTERFACES = "0.0.0.0:6881,[::]:6881";

    explicit TorrentSession(
        std::filesystem::path savePath,
        std::string listenInterfaces = std::string(PUBLIC_INTERFACES),
        bool discovery = true);

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
        const std::filesystem::path& sealedManifestPath) override;

    [[nodiscard]] const std::vector<domain::SeedEntry>& Entries() const override;

    [[nodiscard]] std::uint64_t UploadedBytes() const override;

    [[nodiscard]] std::expected<void, domain::FetchError> Begin(
        std::string identifier,
        const domain::ChunkDigest& infoHash,
        const std::filesystem::path& stagingFolder,
        const std::vector<std::string>& wantedFiles) override;

    [[nodiscard]] domain::FetchStatus Fetch() const override;

    void Cancel() override;

private:
    struct SeededTorrent {
        std::string identifier;
        std::shared_ptr<libtorrent::torrent_handle> handle;
    };

    [[nodiscard]] bool AlreadySeeding_(const std::string& identifier) const;

    void RefreshEntries_();

    void ApplyFetchPriorities_();

    void RefreshFetch_();

    ChunkLocator _locator;
    std::unique_ptr<libtorrent::session> _session;
    std::filesystem::path _savePath;
    domain::SwarmStatus _status;
    std::vector<SeededTorrent> _seeded;
    std::vector<domain::SeedEntry> _entries;
    bool _enabled;
    std::unique_ptr<libtorrent::torrent_handle> _fetching;
    std::set<std::string> _wanted;
    bool _prioritised;
    domain::FetchStatus _fetch;
};

}
