#pragma once

#include "domain/interfaces/platform/IFilePicker.h"

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::gui {
class Win32FilePicker final : public domain::IFilePicker {
public:
	explicit Win32FilePicker(HWND owner);

	~Win32FilePicker() override;

	[[nodiscard]] std::optional<std::filesystem::path> SaveFile(
		std::string_view title,
		std::string_view suggestedName,
		std::string_view filterLabel,
		std::string_view filterPattern
	) const override;

	[[nodiscard]] std::optional<std::filesystem::path> OpenFile(
		std::string_view title,
		std::string_view filterLabel,
		std::string_view filterPattern
	) const override;

	[[nodiscard]] std::optional<std::filesystem::path> PickFolder(
		std::string_view title
	) const override;

	void SetOwner(HWND owner);

private:
	HWND _owner;
};
}
