#include "downloader/storage/TransferBudget.h"

#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
TransferBudget::TransferBudget()
	: _folder() {}

void TransferBudget::UseFolder(std::filesystem::path folder) {
	_folder = std::move(folder);
}

std::optional<std::int64_t> TransferBudget::Load() const {
	if (_folder.empty()) {
		return std::nullopt;
	}

	std::ifstream input(_folder / FILE_NAME, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	std::int64_t stored = 0;
	input >> stored;

	if (!input && !input.eof()) {
		return std::nullopt;
	}

	if (stored < UNLIMITED || stored > MAXIMUM_BYTES_PER_SECOND) {
		return std::nullopt;
	}

	return stored;
}

void TransferBudget::Save(const std::int64_t bytesPerSecond) const {
	if (_folder.empty()) {
		return;
	}

	if (bytesPerSecond < UNLIMITED || bytesPerSecond > MAXIMUM_BYTES_PER_SECOND) {
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

	output << bytesPerSecond;
}
}
