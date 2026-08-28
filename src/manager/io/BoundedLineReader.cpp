#include "manager/io/BoundedLineReader.h"

#include <fstream>

namespace wgrd::manager {
std::optional<std::string> BoundedLineReader::Read(
	const std::filesystem::path& path,
	const std::size_t limit
) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	std::string line;
	line.reserve(limit);

	char character = '\0';

	while (input.get(character)) {
		if (character == '\n') {
			break;
		}

		if (line.size() == limit) {
			return std::nullopt;
		}

		line.push_back(character);
	}

	if (!line.empty() && line.back() == '\r') {
		line.pop_back();
	}

	return line;
}
}
