#pragma once

#include "domain/types/content/ModManifest.h"

#include <filesystem>
#include <string>

namespace wgrd::downloader {
class ModFolderStamp {
public:
	static constexpr std::string_view MISSING_MARK = "missing";

	[[nodiscard]] static std::string Compute(
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder
	);
};
}
