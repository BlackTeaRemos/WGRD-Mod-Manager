#include "downloader/storage/SeedStampStore.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
SeedStampStore::SeedStampStore(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

void SeedStampStore::UseFolder(std::filesystem::path folder) {
	_folder = std::move(folder);
}

bool SeedStampStore::IsAcceptableKey_(const std::string_view key) {
	if (key.empty() || key.size() > 128) {
		return false;
	}

	return std::ranges::all_of(key, [](const char character) {
			return (character >= '0' && character <= '9')
			       || (character >= 'a' && character <= 'f')
			       || (character >= 'A' && character <= 'F');
		}
	);
}

std::filesystem::path SeedStampStore::PathFor_(const std::string_view key) const {
	return _folder / (std::string(key) + std::string(SUFFIX));
}

std::optional<std::string> SeedStampStore::Load(const std::string_view key) const {
	if (!IsAcceptableKey_(key)) {
		return std::nullopt;
	}

	const std::filesystem::path source = PathFor_(key);

	std::error_code failure;
	const std::uintmax_t size = std::filesystem::file_size(source, failure);

	if (failure || size == 0 || size > MAXIMUM_BYTES) {
		return std::nullopt;
	}

	std::ifstream input(source, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	const std::istreambuf_iterator<char> begin(input);
	const std::istreambuf_iterator<char> end;

	return std::string(begin, end);
}

bool SeedStampStore::Save(const std::string_view key, const std::string& stamp) const {
	if (!IsAcceptableKey_(key) || stamp.empty() || stamp.size() > MAXIMUM_BYTES) {
		return false;
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);

	if (failure) {
		return false;
	}

	std::ofstream output(PathFor_(key), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output.write(stamp.data(), static_cast<std::streamsize>(stamp.size()));

	return static_cast<bool>(output);
}

void SeedStampStore::Forget(const std::string_view key) const {
	if (!IsAcceptableKey_(key)) {
		return;
	}

	std::error_code failure;
	std::filesystem::remove(PathFor_(key), failure);
}
}
