#pragma once

#include <libtorrent/settings_pack.hpp>

#include <string>

namespace wgrd::downloader {
class SessionSettings {
public:
	SessionSettings() = delete;

	[[nodiscard]] static libtorrent::settings_pack Build(
		const std::string& listenInterfaces,
		bool discovery
	);

private:
	[[nodiscard]] static int AlertMask_();
};
}
