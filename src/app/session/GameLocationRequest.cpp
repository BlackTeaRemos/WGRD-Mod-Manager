#include "app/session/GameLocationRequest.h"

#include "manager/environment/GameLocator.h"

#include <utility>

namespace wgrd::app {
std::expected<void, domain::GameLocationError> GameLocationRequest::SelectRoot(const std::filesystem::path& root) {
	const auto installation = manager::GameLocator::FromRoot(root);
	if (!installation.has_value()) {
		return std::unexpected(domain::GameLocationError::InvalidFolder);
	}
	_pending = *installation;
	return {};
}

void GameLocationRequest::CancelSelection() {
	_pending.reset();
}

std::optional<domain::GameInstallation> GameLocationRequest::Take() {
	return std::exchange(_pending, std::nullopt);
}

std::optional<domain::GameInstallation> GameLocationRequest::Detect() {
	if (auto installation = manager::GameLocator::Resolve()) {
		return installation;
	}
	const auto detected = manager::GameLocator::Detect();
	return detected.empty() ? std::nullopt : std::optional<domain::GameInstallation>(detected.front());
}
}
