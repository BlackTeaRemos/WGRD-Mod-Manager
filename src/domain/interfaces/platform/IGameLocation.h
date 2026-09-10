#pragma once

#include "domain/interfaces/platform/GameLocationError.h"

#include <expected>
#include <filesystem>

namespace wgrd::domain {
class IGameLocation {
public:
	virtual ~IGameLocation() = 0;
	virtual void CancelSelection() = 0;
	[[nodiscard]] virtual std::expected<void, GameLocationError> SelectRoot(const std::filesystem::path& root) = 0;
};

inline IGameLocation::~IGameLocation() = default;
}
