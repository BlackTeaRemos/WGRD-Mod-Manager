#pragma once

#include <array>
#include <string_view>

namespace wgrd::manager {
class DownloadHostPolicy {
public:
	static constexpr std::string_view SCHEME = "https://";

	static constexpr std::array<std::string_view, 4> ALLOWED_HOSTS{
		"github.com",
		"api.github.com",
		"raw.githubusercontent.com",
		"objects.githubusercontent.com"
	};

	[[nodiscard]] static bool Accepts(std::string_view url);

	[[nodiscard]] static std::string_view HostOf(std::string_view url);

private:
	DownloadHostPolicy() = delete;
};
}
