#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wgrd::domain {
struct BaselineIndexStatus {
	std::size_t indexedFiles = 0;
	std::size_t failedFiles = 0;
	std::size_t totalFiles = 0;
	std::uint64_t indexedBytes = 0;
	std::uint64_t totalBytes = 0;
	std::uint64_t activeBytes = 0;
	std::string activeFile;
	bool running = false;
	bool boosted = false;

	[[nodiscard]] bool Complete() const {
		return !running && indexedFiles + failedFiles == totalFiles;
	}
};
}
