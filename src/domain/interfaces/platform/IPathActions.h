#pragma once

#include "domain/interfaces/platform/PathActionError.h"

#include <expected>
#include <filesystem>

namespace wgrd::domain {
class IPathActions {
public:
	virtual ~IPathActions() = 0;
	[[nodiscard]] virtual std::expected<void, PathActionError> OpenFolder(const std::filesystem::path& path) const = 0;
};

inline IPathActions::~IPathActions() = default;
}
