#include "manager/mirror/MirrorSwitch.h"

#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace wgrd::manager {
MirrorSwitch::MirrorSwitch(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

std::optional<bool> MirrorSwitch::Load() const {
	if (_folder.empty()) {
		return std::nullopt;
	}

	std::ifstream input(_folder / FILE_NAME, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	std::string mark;
	input >> mark;

	if (mark == MIRRORING_MARK) {
		return true;
	}

	if (mark == IDLE_MARK) {
		return false;
	}

	return std::nullopt;
}

void MirrorSwitch::Save(const bool enabled) const {
	if (_folder.empty()) {
		return;
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);

	if (failure) {
		return;
	}

	std::ofstream output(_folder / FILE_NAME, std::ios::binary | std::ios::trunc);
	if (!output) {
		return;
	}

	output << (enabled ? MIRRORING_MARK : IDLE_MARK);
}
}
