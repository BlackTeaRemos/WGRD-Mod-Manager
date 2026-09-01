#include "downloader/storage/SeedingSwitch.h"

#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
SeedingSwitch::SeedingSwitch()
	: _folder() {}

void SeedingSwitch::UseFolder(std::filesystem::path folder) {
	_folder = std::move(folder);
}

std::optional<bool> SeedingSwitch::Load() const {
	if (_folder.empty()) {
		return std::nullopt;
	}

	std::ifstream input(_folder / FILE_NAME, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	std::string mark;
	input >> mark;

	if (mark == PAUSED_MARK) {
		return false;
	}

	if (mark == RUNNING_MARK) {
		return true;
	}

	return std::nullopt;
}

void SeedingSwitch::Save(const bool enabled) const {
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

	output << (enabled ? RUNNING_MARK : PAUSED_MARK);
}
}
