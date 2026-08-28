#include "domain/types/order/InstallFolder.h"

namespace wgrd::domain {
std::expected<InstallFolder, InstallFolderError> InstallFolder::Parse(const std::string_view text) {
	if (text.empty()) {
		return std::unexpected(InstallFolderError::Empty);
	}
	if (text.front() == '#') {
		return std::unexpected(InstallFolderError::CommentMarker);
	}
	if (text.front() == ' ' || text.front() == '\t') {
		return std::unexpected(InstallFolderError::LeadingWhitespace);
	}
	if (text.back() == ' ' || text.back() == '\t') {
		return std::unexpected(InstallFolderError::TrailingWhitespace);
	}
	if (text.find('/') != std::string_view::npos || text.find('\\') != std::string_view::npos) {
		return std::unexpected(InstallFolderError::PathSeparator);
	}
	if (text == "." || text == "..") {
		return std::unexpected(InstallFolderError::RelativeMarker);
	}
	if (text.front() == '.') {
		return std::unexpected(InstallFolderError::HiddenFolder);
	}

	return InstallFolder(std::string(text));
}

const std::string& InstallFolder::Value() const noexcept {
	return _value;
}

InstallFolder::InstallFolder(std::string value)
	: _value(std::move(value)) {}
}
