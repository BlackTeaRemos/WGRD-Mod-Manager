#include "manager/baseline/records/BaselineRecordStore.h"

#include "manager/baseline/records/BaselineRecordCodec.h"

#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace wgrd::manager {
BaselineRecordStore::BaselineRecordStore(std::filesystem::path folder, const domain::IContentHasher& hasher)
	: _folder(std::move(folder))
	, _hasher(&hasher) {}

std::filesystem::path BaselineRecordStore::PathFor_(const std::string_view relativePath) const {
	const std::span<const std::byte> bytes(
		reinterpret_cast<const std::byte*>(relativePath.data()),
		relativePath.size()
	);

	return _folder / (_hasher->Hash(bytes).ToHex() + std::string(FILE_EXTENSION));
}

std::vector<BaselineFileRecord> BaselineRecordStore::LoadAll() const {
	std::vector<BaselineFileRecord> records;

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
		if (failure || records.size() >= RECORD_LIMIT) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != FILE_EXTENSION) {
			continue;
		}

		std::ifstream input(walker->path(), std::ios::binary);
		if (!input) {
			continue;
		}

		const std::uintmax_t size = walker->file_size(failure);
		if (failure || size > BaselineRecordCodec::DOCUMENT_LIMIT) {
			continue;
		}

		std::string document(static_cast<std::size_t>(size), '\0');
		input.read(document.data(), static_cast<std::streamsize>(document.size()));
		if (!input || input.peek() != std::char_traits<char>::eof()) {
			continue;
		}

		std::optional<BaselineFileRecord> decoded = BaselineRecordCodec::Decode(document);
		if (!decoded.has_value()) {
			continue;
		}

		records.push_back(std::move(*decoded));
	}

	return records;
}

bool BaselineRecordStore::Save(const BaselineFileRecord& record) const {
	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);
	if (failure) {
		return false;
	}

	std::ofstream output(PathFor_(record.relativePath), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output << BaselineRecordCodec::Encode(record) << "\n";

	return static_cast<bool>(output);
}

bool BaselineRecordStore::Remove(const std::string_view relativePath) const {
	std::error_code failure;
	const bool removed = std::filesystem::remove(PathFor_(relativePath), failure);

	return removed && !failure;
}
}
