#pragma once

#include "manager/install/ContentInstaller.h"
#include "manager/text/ServiceText.h"

#include <string_view>

namespace wgrd::manager {
[[nodiscard]] constexpr std::string_view DescribeInstallError(const InstallError failure) {
	switch (failure) {
		case InstallError::FolderUnwritable:
			return text::INSTALL_FOLDER_UNWRITABLE;
		case InstallError::HeldChunkMissing:
			return text::INSTALL_HELD_MISSING;
		case InstallError::HeldChunkUnreadable:
			return text::INSTALL_HELD_UNREADABLE;
		case InstallError::RemoteChunkUnavailable:
			return text::INSTALL_REMOTE_UNAVAILABLE;
		case InstallError::RemoteChunkCorrupt:
			return text::INSTALL_REMOTE_CORRUPT;
		case InstallError::WriteFailed:
			return text::INSTALL_WRITE_FAILED;
		case InstallError::SwapFailed:
			return text::INSTALL_SWAP_FAILED;
	}

	return text::INSTALL_FAILED;
}
}
