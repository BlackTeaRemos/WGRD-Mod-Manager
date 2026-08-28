#include "manager/patcher/PatcherRuntimeVersion.h"

#include "manager/io/BoundedLineReader.h"

#include <algorithm>
#include <optional>
#include <string>

namespace wgrd::manager {
bool PatcherRuntimeVersion::Printable_(const std::string_view text) {
	return std::ranges::all_of(text, [](const char character) {
			return static_cast<unsigned char>(character) >= 0x20
			       && static_cast<unsigned char>(character) < 0x7F;
		}
	);
}

std::string PatcherRuntimeVersion::Read(const std::filesystem::path& modsFolder) {
	const std::optional<std::string> stamp = BoundedLineReader::Read(
		modsFolder / std::string(FILE_NAME),
		LENGTH_LIMIT
	);

	if (!stamp.has_value() || stamp->empty() || !Printable_(*stamp)) {
		return {};
	}

	return *stamp;
}
}
