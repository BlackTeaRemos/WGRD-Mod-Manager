#include "manager/announce/AnnounceStore.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace wgrd::manager {
AnnounceStore::AnnounceStore(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

AnnounceStore::~AnnounceStore() = default;

std::filesystem::path AnnounceStore::PathFor_(const domain::SignedAnnounce& announce) const {
	return _folder / (announce.publisher.ToHex() + "_" + announce.modName + std::string(FILE_EXTENSION));
}

bool AnnounceStore::Save(
	const domain::SignedAnnounce& announce,
	const std::span<const std::uint8_t> record
) const {
	if (record.size() != RECORD_BYTES) {
		return false;
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);
	if (failure) {
		return false;
	}

	std::ofstream output(PathFor_(announce), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output.write(
		reinterpret_cast<const char*>(record.data()),
		static_cast<std::streamsize>(record.size())
	);

	return static_cast<bool>(output);
}

std::vector<std::vector<std::uint8_t>> AnnounceStore::LoadAll() const {
	std::vector<std::vector<std::uint8_t>> records;

	std::error_code failure;
	if (!std::filesystem::is_directory(_folder, failure)) {
		return records;
	}

	std::filesystem::directory_iterator walker(_folder, failure);
	if (failure) {
		return records;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || records.size() >= MAXIMUM_RECORDS) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != FILE_EXTENSION) {
			continue;
		}

		if (std::filesystem::file_size(walker->path(), failure) != RECORD_BYTES || failure) {
			continue;
		}

		std::ifstream input(walker->path(), std::ios::binary);
		if (!input) {
			continue;
		}

		const std::string raw(
			(std::istreambuf_iterator<char>(input)),
			std::istreambuf_iterator<char>()
		);

		if (raw.size() != RECORD_BYTES) {
			continue;
		}

		std::vector<std::uint8_t> record;
		record.reserve(RECORD_BYTES);
		for (const char value : raw) {
			record.push_back(static_cast<std::uint8_t>(value));
		}

		records.push_back(std::move(record));
	}

	return records;
}
}
