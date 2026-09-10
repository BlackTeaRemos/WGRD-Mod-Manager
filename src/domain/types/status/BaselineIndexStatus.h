#pragma once

#include <cstddef>
#include <cstdint>

namespace wgrd::domain {
struct BaselineIndexStatus {
	std::size_t indexedFiles = 0;
	std::size_t failedFiles = 0;
	std::size_t totalFiles = 0;
	std::uint64_t indexedBytes = 0;
	std::uint64_t totalBytes = 0;
	bool running = false;
	bool boosted = false;

	[[nodiscard]] bool Complete() const {
		return !running && indexedFiles + failedFiles == totalFiles;
	}
};
}
