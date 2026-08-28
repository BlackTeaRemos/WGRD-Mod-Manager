#include "downloader/transfer/TorrentSession.h"

#include "downloader/storage/InstalledFolderStorage.h"
#include "downloader/torrent/build/VirtualChunkSetTorrent.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::downloader {

namespace {

int AlertMask() {
    const libtorrent::alert_category_t mask =
        libtorrent::alert_category::status |
        libtorrent::alert_category::dht |
        libtorrent::alert_category::error;

    return static_cast<int>(static_cast<std::uint32_t>(mask));
}

libtorrent::settings_pack BuildSettings(const std::string& listenInterfaces, bool discovery) {
    libtorrent::settings_pack settings;

    settings.set_bool(libtorrent::settings_pack::enable_dht, discovery);
    settings.set_bool(libtorrent::settings_pack::enable_lsd, discovery);
    settings.set_bool(libtorrent::settings_pack::enable_upnp, discovery);
    settings.set_bool(libtorrent::settings_pack::enable_natpmp, discovery);

    settings.set_str(libtorrent::settings_pack::listen_interfaces, listenInterfaces);
    settings.set_int(libtorrent::settings_pack::alert_mask, AlertMask());

    settings.set_str(libtorrent::settings_pack::user_agent, "wgrd-mod-manager");
    settings.set_str(libtorrent::settings_pack::peer_fingerprint, "-WG0100-");

    return settings;
}

}

TorrentSession::TorrentSession(
    std::filesystem::path savePath,
    std::string listenInterfaces,
    bool discovery)
    : _locator(),
      _session(nullptr),
      _savePath(std::move(savePath)),
      _status(),
      _seeded(),
      _entries(),
      _enabled(true),
      _fetching(nullptr),
      _wanted(),
      _prioritised(false),
      _fetch() {

    libtorrent::session_params parameters(BuildSettings(listenInterfaces, discovery));

    parameters.disk_io_constructor = [this](
        libtorrent::io_context& context,
        const libtorrent::settings_interface&,
        libtorrent::counters&) -> std::unique_ptr<libtorrent::disk_interface> {
        return std::make_unique<InstalledFolderStorage>(_locator, context);
    };

    _session = std::make_unique<libtorrent::session>(std::move(parameters));

    _status.running = true;
    _status.listenInterface = listenInterfaces;

    _session->post_dht_stats();
}

TorrentSession::~TorrentSession() = default;

std::uint16_t TorrentSession::ListenPort() const {
    return static_cast<std::uint16_t>(_session->listen_port());
}

void TorrentSession::ConnectLocalPeer(std::uint16_t port) {
    if (_fetching == nullptr || !_fetching->is_valid()) {
        return;
    }

    _fetching->connect_peer(libtorrent::tcp::endpoint(
        libtorrent::make_address_v4("127.0.0.1"),
        port));
}

const domain::SwarmStatus& TorrentSession::Status() const {
    return _status;
}

bool TorrentSession::Enabled() const {
    return _enabled;
}

void TorrentSession::SetEnabled(bool enabled) {
    _enabled = enabled;

    if (_session == nullptr) {
        return;
    }

    for (const SeededTorrent& seeded : _seeded) {
        if (!seeded.handle->is_valid()) {
            continue;
        }

        if (enabled) {
            seeded.handle->resume();
        } else {
            seeded.handle->pause();
        }
    }
}

bool TorrentSession::AlreadySeeding_(const std::string& identifier) const {
    return std::any_of(
        _seeded.begin(),
        _seeded.end(),
        [&identifier](const SeededTorrent& seeded) {
            return seeded.identifier == identifier;
        });
}

std::expected<domain::SeedEntry, domain::SeedError> TorrentSession::Announce(
    const domain::ModManifest& manifest,
    const std::filesystem::path& modFolder,
    const std::filesystem::path& sealedManifestPath) {

    if (!_enabled) {
        return std::unexpected(domain::SeedError::Disabled);
    }

    const std::string identifier = manifest.Identifier();

    if (AlreadySeeding_(identifier)) {
        return std::unexpected(domain::SeedError::AlreadySeeding);
    }

    _locator.Register(manifest, modFolder);

    std::vector<std::uint8_t> sealed;

    {
        std::ifstream input(sealedManifestPath, std::ios::binary);
        if (input) {
            sealed.assign(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
        }
    }

    if (sealed.empty()) {
        return std::unexpected(domain::SeedError::TorrentBuildFailed);
    }

    _locator.RegisterFile(
        std::string(domain::ChunkFileNaming::MANIFEST_FILE),
        sealedManifestPath,
        0,
        sealed.size());

    const auto torrent = VirtualChunkSetTorrent::Create(
        manifest,
        modFolder,
        manifest.TorrentName(),
        sealed);

    if (!torrent.has_value()) {
        return std::unexpected(domain::SeedError::TorrentBuildFailed);
    }

    libtorrent::error_code parsing;
    libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
        libtorrent::span<const char>(
            torrent->bencoded.data(),
            static_cast<std::ptrdiff_t>(torrent->bencoded.size())),
        parsing,
        libtorrent::load_torrent_limits{});

    if (parsing || parameters.ti == nullptr) {
        return std::unexpected(domain::SeedError::TorrentBuildFailed);
    }

    parameters.save_path = _savePath.string();
    parameters.flags &= ~libtorrent::torrent_flags::paused;
    parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

    libtorrent::error_code adding;
    libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

    if (adding || !handle.is_valid()) {
        return std::unexpected(domain::SeedError::SessionRejected);
    }

    _seeded.push_back(SeededTorrent{
        identifier,
        std::make_shared<libtorrent::torrent_handle>(std::move(handle))
    });

    const domain::SeedEntry entry{
        identifier,
        manifest.ModName(),
        manifest.Version(),
        torrent->payloadBytes,
        manifest.ChunkCount(),
        torrent->infoHash,
        false,
        0,
        0
    };

    _entries.push_back(entry);

    return entry;
}

const std::vector<domain::SeedEntry>& TorrentSession::Entries() const {
    return _entries;
}

std::uint64_t TorrentSession::UploadedBytes() const {
    std::uint64_t total = 0;
    for (const domain::SeedEntry& entry : _entries) {
        total += entry.uploadedBytes;
    }
    return total;
}

void TorrentSession::RefreshEntries_() {
    for (std::size_t index = 0; index < _seeded.size() && index < _entries.size(); ++index) {
        const libtorrent::torrent_handle& handle = *_seeded[index].handle;

        if (!handle.is_valid()) {
            continue;
        }

        const libtorrent::torrent_status status = handle.status();

        _entries[index].seeding = status.is_seeding;
        _entries[index].peers = static_cast<std::uint32_t>(status.num_peers);
        _entries[index].uploadedBytes = static_cast<std::uint64_t>(status.total_upload);
    }
}

std::expected<void, domain::FetchError> TorrentSession::Begin(
    std::string identifier,
    const domain::ChunkDigest& infoHash,
    const std::filesystem::path& stagingFolder,
    const std::vector<std::string>& wantedFiles) {

    if (_fetch.Busy()) {
        return std::unexpected(domain::FetchError::Busy);
    }

    if (wantedFiles.empty()) {
        return std::unexpected(domain::FetchError::NothingWanted);
    }

    Cancel();

    _wanted.clear();
    for (const std::string& fileName : wantedFiles) {
        _wanted.insert(fileName);
    }

    const std::string magnet = "magnet:?xt=urn:btmh:1220" + infoHash.ToHex();

    libtorrent::error_code parsing;
    libtorrent::add_torrent_params parameters = libtorrent::parse_magnet_uri(magnet, parsing);

    if (parsing) {
        return std::unexpected(domain::FetchError::MagnetRejected);
    }

    std::error_code creating;
    std::filesystem::create_directories(stagingFolder, creating);

    parameters.save_path = stagingFolder.string();
    parameters.flags |= libtorrent::torrent_flags::default_dont_download;
    parameters.flags &= ~libtorrent::torrent_flags::paused;
    parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

    libtorrent::error_code adding;
    libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

    if (adding || !handle.is_valid()) {
        return std::unexpected(domain::FetchError::SessionRejected);
    }

    _fetching = std::make_unique<libtorrent::torrent_handle>(std::move(handle));
    _prioritised = false;

    _fetch = domain::FetchStatus{};
    _fetch.phase = domain::FetchPhase::Metadata;
    _fetch.identifier = std::move(identifier);
    _fetch.stagingFolder = stagingFolder;

    return {};
}

domain::FetchStatus TorrentSession::Fetch() const {
    return _fetch;
}

void TorrentSession::Cancel() {
    if (_fetching != nullptr && _fetching->is_valid() && _session != nullptr) {
        _session->remove_torrent(*_fetching);
    }

    _fetching.reset();
    _prioritised = false;
    _wanted.clear();

    if (_fetch.Busy()) {
        _fetch.phase = domain::FetchPhase::Idle;
    }
}

void TorrentSession::ApplyFetchPriorities_() {
    if (_fetching == nullptr || _prioritised || !_fetching->is_valid()) {
        return;
    }

    const std::shared_ptr<const libtorrent::torrent_info> info = _fetching->torrent_file();
    if (info == nullptr) {
        return;
    }

    const libtorrent::file_storage& files = info->layout();

    std::vector<libtorrent::download_priority_t> priorities;
    priorities.reserve(static_cast<std::size_t>(info->num_files()));

    std::uint64_t wantedBytes = 0;

    for (const libtorrent::file_index_t index : files.file_range()) {
        if (files.pad_file_at(index)) {
            priorities.push_back(libtorrent::dont_download);
            continue;
        }

        const std::string leaf(domain::ChunkFileNaming::LeafOf(files.file_path(index)));

        const bool wanted = _wanted.contains(leaf);

        priorities.push_back(wanted ? libtorrent::default_priority : libtorrent::dont_download);

        if (wanted) {
            wantedBytes += static_cast<std::uint64_t>(files.file_size(index));
        }
    }

    _fetching->prioritize_files(priorities);

    _fetch.wantedBytes = wantedBytes;
    _fetch.phase = domain::FetchPhase::Downloading;
    _prioritised = true;
}

void TorrentSession::RefreshFetch_() {
    if (_fetching == nullptr || !_fetching->is_valid()) {
        return;
    }

    ApplyFetchPriorities_();

    const libtorrent::torrent_status status = _fetching->status();

    _fetch.peers = static_cast<std::uint32_t>(status.num_peers);
    _fetch.fetchedBytes = static_cast<std::uint64_t>(status.total_wanted_done);

    if (_prioritised && status.is_finished) {
        _fetch.phase = domain::FetchPhase::Complete;
    }
}

void TorrentSession::Poll() {
    if (_session == nullptr) {
        return;
    }

    std::vector<libtorrent::alert*> alerts;
    _session->pop_alerts(&alerts);

    for (const libtorrent::alert* const entry : alerts) {
        if (const auto* const stats = libtorrent::alert_cast<libtorrent::dht_stats_alert>(entry)) {
            std::uint64_t nodes = 0;
            for (const libtorrent::dht_routing_bucket& bucket : stats->routing_table) {
                nodes += static_cast<std::uint64_t>(bucket.num_nodes);
            }
            _status.dhtNodes = nodes;
        }
    }

    _status.dhtRunning = _session->is_dht_running();
    _status.listenPort = static_cast<std::uint32_t>(_session->listen_port());

    RefreshEntries_();
    RefreshFetch_();

    _session->post_dht_stats();
}

}
