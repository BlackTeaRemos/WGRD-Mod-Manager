#include "gui/state/ApplicationState.h"

#include <utility>

namespace wgrd::gui {
Screen ApplicationState::ActiveScreen() const noexcept {
	return _screen;
}

void ApplicationState::SetScreen(const Screen screen) noexcept {
	_screen = screen;
	_detailTarget.clear();
}

bool ApplicationState::SettingsOpen() const noexcept {
	return _settingsOpen;
}

void ApplicationState::OpenSettings() noexcept {
	_settingsOpen = true;
	_detailTarget.clear();
}

void ApplicationState::CloseSettings() noexcept {
	_settingsOpen = false;
}

bool ApplicationState::DetailOpen() const noexcept {
	return !_detailTarget.empty();
}

const std::string& ApplicationState::DetailTarget() const noexcept {
	return _detailTarget;
}

void ApplicationState::OpenDetail(std::string target) {
	_settingsOpen = false;
	_detailTarget = std::move(target);
}

void ApplicationState::CloseDetail() noexcept {
	_detailTarget.clear();
}

std::size_t ApplicationState::SelectedProfile() const noexcept {
	return _selectedProfile;
}

void ApplicationState::SelectProfile(const std::size_t index) noexcept {
	_selectedProfile = index;
}

std::size_t ApplicationState::PublishStep() const noexcept {
	return _publishStep;
}

void ApplicationState::SetPublishStep(const std::size_t step) noexcept {
	_publishStep = step > 2 ? 2 : step;
}

std::size_t ApplicationState::CatalogFilter() const noexcept {
	return _catalogFilter;
}

void ApplicationState::SetCatalogFilter(const std::size_t filter) noexcept {
	_catalogFilter = filter;
}

bool ApplicationState::ExitRequested() const noexcept {
	return _exitRequested;
}

void ApplicationState::RequestExit() noexcept {
	_exitRequested = true;
}

void ApplicationState::ClearExitRequest() noexcept {
	_exitRequested = false;
}

bool ApplicationState::BackgroundOnClose() const noexcept {
	return _backgroundOnClose;
}

void ApplicationState::SetBackgroundOnClose(const bool enabled) noexcept {
	_backgroundOnClose = enabled;
}

const std::string& ApplicationState::PublishFolder() const noexcept {
	return _publishFolder;
}

void ApplicationState::SelectPublishFolder(std::string folder) {
	_publishFolder = std::move(folder);
}
}
