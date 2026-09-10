#pragma once

#include <cstdint>

namespace wgrd::domain {
struct FileStamp {
	std::uint64_t size = 0;
	std::int64_t modifiedTicks = 0;

	bool operator==(const FileStamp& other) const = default;
};
}
