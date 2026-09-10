#pragma once

#include "domain/interfaces/platform/IPathActions.h"

#include <windows.h>

namespace wgrd::gui {
class Win32PathActions final : public domain::IPathActions {
public:
	explicit Win32PathActions(HWND owner);
	~Win32PathActions() override = default;
	void SetOwner(HWND owner);
	[[nodiscard]] std::expected<void, domain::PathActionError> OpenFolder(const std::filesystem::path& path) const override;

private:
	HWND _owner;
};
}
