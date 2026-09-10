#pragma once

#include <libtorrent/settings_pack.hpp>

#include <string>
#include <string_view>

namespace wgrd::downloader {
class SessionSettings {
public:
	static constexpr std::string_view LOOPBACK_INTERFACES = "127.0.0.1:6881";

	SessionSettings() = delete;

	[[nodiscard]] static libtorrent::settings_pack Build(
		const std::string& listenInterfaces,
		bool discovery
	);

private:
	[[nodiscard]] static int AlertMask_();
};
}
