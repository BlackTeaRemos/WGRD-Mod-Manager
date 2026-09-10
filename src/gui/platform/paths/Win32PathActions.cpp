#include "gui/platform/paths/Win32PathActions.h"

#include <shellapi.h>
#include <system_error>

namespace wgrd::gui {
Win32PathActions::Win32PathActions(const HWND owner)
	: _owner(owner) {}

void Win32PathActions::SetOwner(const HWND owner) {
	_owner = owner;
}

std::expected<void, domain::PathActionError> Win32PathActions::OpenFolder(const std::filesystem::path& path) const {
	std::error_code failure;
	if (!std::filesystem::is_directory(path, failure) || failure) {
		return std::unexpected(domain::PathActionError::FolderMissing);
	}
	const auto absolute = std::filesystem::absolute(path, failure);
	if (failure) {
		return std::unexpected(domain::PathActionError::FolderMissing);
	}
	const HINSTANCE result = ShellExecuteW(_owner, L"open", absolute.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<INT_PTR>(result) <= 32) {
		return std::unexpected(domain::PathActionError::LaunchFailed);
	}
	return {};
}
}
