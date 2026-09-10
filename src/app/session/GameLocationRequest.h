#pragma once

#include "domain/interfaces/platform/IGameLocation.h"
#include "domain/types/game/GameInstallation.h"

#include <optional>

namespace wgrd::app {
class GameLocationRequest final : public domain::IGameLocation {
public:
	GameLocationRequest() = default;
	~GameLocationRequest() override = default;
	void CancelSelection() override;
	[[nodiscard]] std::expected<void, domain::GameLocationError> SelectRoot(const std::filesystem::path& root) override;
	[[nodiscard]] std::optional<domain::GameInstallation> Take();
	[[nodiscard]] static std::optional<domain::GameInstallation> Detect();

private:
	std::optional<domain::GameInstallation> _pending;
};
}
