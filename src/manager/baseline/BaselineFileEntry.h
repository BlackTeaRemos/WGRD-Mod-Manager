#pragma once

#include "domain/types/content/FileStamp.h"

#include <filesystem>
#include <string>

namespace wgrd::manager {
struct BaselineFileEntry {
	std::string relativePath;
	std::filesystem::path file;
	domain::FileStamp stamp;
};
}
