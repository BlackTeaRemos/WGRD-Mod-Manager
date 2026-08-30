#pragma once

#include "domain/interfaces/platform/IUriLauncher.h"

#include <string_view>

namespace wgrd::gui {
class Win32UriLauncher final : public domain::IUriLauncher {
public:
	static constexpr std::string_view SCHEME = "https://";
	static constexpr std::size_t URI_LIMIT = 2048;

	Win32UriLauncher();

	~Win32UriLauncher() override;

	[[nodiscard]] bool Open(std::string_view uri) const override;

private:
	[[nodiscard]] static bool Acceptable_(std::string_view uri);
};
}
