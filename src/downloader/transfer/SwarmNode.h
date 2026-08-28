#pragma once

#include "downloader/storage/ChunkLocator.h"

#include <libtorrent/session_params.hpp>

#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace libtorrent {
class session;
struct torrent_handle;
}

namespace wgrd::downloader {
enum class SwarmNodeError {
	TorrentRejected, NotListening, AlreadyLoaded
};

class SwarmNode {
public:
	explicit SwarmNode(std::filesystem::path savePath);

	SwarmNode(std::filesystem::path savePath, const ChunkLocator& locator);

	SwarmNode(const SwarmNode&) = delete;

	SwarmNode& operator=(const SwarmNode&) = delete;

	~SwarmNode();

	[[nodiscard]] std::uint16_t ListenPort() const;

	[[nodiscard]] std::expected<void, SwarmNodeError> Load(
		std::span<const char> torrentBytes,
		bool seeding
	);

	void ConnectLoopbackPeer(std::uint16_t port);

	[[nodiscard]] bool WaitForCompletion(
		std::chrono::milliseconds timeout,
		SwarmNode* companion = nullptr,
		std::uint16_t redialPort = 0
	);

	void Pump();

	[[nodiscard]] float Progress() const;

	[[nodiscard]] const std::vector<std::string>& Messages() const;

private:
	void DrainAlerts_();

	void AwaitListener_();

	std::unique_ptr<libtorrent::session> _session;
	std::unique_ptr<libtorrent::torrent_handle> _handle;
	std::filesystem::path _savePath;
	std::uint16_t _listenPort;
	bool _cacheFlushed;
	std::vector<std::string> _messages;
};
}
