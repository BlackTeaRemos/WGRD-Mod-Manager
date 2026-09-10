#pragma once

#include "domain/types/content/FileStamp.h"
#include "domain/types/content/ManifestChunk.h"

#include <string>
#include <vector>

namespace wgrd::manager {
struct BaselineFileRecord {
	std::string relativePath;
	domain::FileStamp stamp;
	std::vector<domain::ManifestChunk> chunks;
};
}
