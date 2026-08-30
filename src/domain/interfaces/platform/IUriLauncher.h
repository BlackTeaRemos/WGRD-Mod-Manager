#pragma once

#include <string_view>

namespace wgrd::domain {
class IUriLauncher {
public:
	virtual ~IUriLauncher() = 0;

	[[nodiscard]] virtual bool Open(std::string_view uri) const = 0;
};

inline IUriLauncher::~IUriLauncher() = default;
}
