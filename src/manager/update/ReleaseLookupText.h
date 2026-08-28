#pragma once

#include "manager/update/GitHubReleaseSource.h"

#include <string_view>

namespace wgrd::manager {
class ReleaseLookupText {
public:
	[[nodiscard]] static std::string_view Describe(ReleaseLookupError failure);

private:
	ReleaseLookupText() = delete;
};
}
