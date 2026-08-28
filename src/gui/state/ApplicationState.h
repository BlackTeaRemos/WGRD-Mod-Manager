#pragma once

#include <string>

namespace wgrd::gui {
enum class Screen {
	Catalog
	, Order
	, Transfers
	, Profiles
	, Publish
};

class ApplicationState {
public:
	[[nodiscard]] Screen ActiveScreen() const noexcept;
	void SetScreen(Screen screen) noexcept;

	[[nodiscard]] bool SettingsOpen() const noexcept;
	void OpenSettings() noexcept;
	void CloseSettings() noexcept;

	[[nodiscard]] bool DetailOpen() const noexcept;
	[[nodiscard]] const std::string& DetailTarget() const noexcept;
	void OpenDetail(std::string target);
	void CloseDetail() noexcept;

	[[nodiscard]] std::size_t SelectedProfile() const noexcept;
	void SelectProfile(std::size_t index) noexcept;

	[[nodiscard]] std::size_t PublishStep() const noexcept;
	void SetPublishStep(std::size_t step) noexcept;

	[[nodiscard]] std::size_t CatalogFilter() const noexcept;
	void SetCatalogFilter(std::size_t filter) noexcept;

	[[nodiscard]] const std::string& PublishFolder() const noexcept;
	void SelectPublishFolder(std::string folder);

	[[nodiscard]] bool ExitRequested() const noexcept;
	void RequestExit() noexcept;

private:
	Screen _screen = Screen::Catalog;
	bool _settingsOpen = false;
	std::string _detailTarget;
	std::size_t _selectedProfile = 0;
	std::size_t _publishStep = 0;
	std::size_t _catalogFilter = 0;
	std::string _publishFolder;
	bool _exitRequested = false;
};
}
