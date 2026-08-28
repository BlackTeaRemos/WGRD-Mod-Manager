#include "manager/patcher/PatcherMarker.h"

#include "manager/io/BoundedLineReader.h"

#include <fstream>
#include <optional>
#include <system_error>
#include <utility>

namespace wgrd::manager {
PatcherMarker::PatcherMarker(std::filesystem::path dataDirectory)
	: _dataDirectory(std::move(dataDirectory)) {}

PatcherMarker::~PatcherMarker() = default;

std::filesystem::path PatcherMarker::PathFor_() const {
	return _dataDirectory / std::string(FILE_NAME);
}

std::string PatcherMarker::Read() const {
	const std::optional<std::string> tag = BoundedLineReader::Read(PathFor_(), TAG_LIMIT);
	if (!tag.has_value()) {
		return {};
	}

	return *tag;
}

bool PatcherMarker::Write(const std::string_view tag) const {
	if (tag.empty() || tag.size() > TAG_LIMIT) {
		return false;
	}

	std::error_code failure;
	std::filesystem::create_directories(_dataDirectory, failure);
	if (failure) {
		return false;
	}

	std::ofstream output(PathFor_(), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output << tag;

	return static_cast<bool>(output);
}
}
