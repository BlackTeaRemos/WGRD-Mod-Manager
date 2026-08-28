#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <string>
#include <string_view>

namespace wgrd::domain {
class ChunkFileNaming {
public:
	static constexpr std::string_view SUFFIX = ".chunk";
	static constexpr std::string_view MANIFEST_FILE = "manifest.wgrdm";

	[[nodiscard]] static std::string FileNameFor(const ChunkDigest& digest) {
		return digest.ToHex() + std::string(SUFFIX);
	}

	[[nodiscard]] static std::string_view LeafOf(const std::string_view fileName) {
		const std::size_t separator = fileName.find_last_of("/\\");
		return separator == std::string_view::npos
		       ? fileName
		       : fileName.substr(separator + 1);
	}

	[[nodiscard]] static std::string DigestFromFileName(const std::string_view fileName) {
		const std::size_t separator = fileName.find_last_of("/\\");
		const std::string_view leaf = separator == std::string_view::npos
		                              ? fileName
		                              : fileName.substr(separator + 1);

		if (!leaf.ends_with(SUFFIX)) {
			return {};
		}

		return std::string(leaf.substr(0, leaf.size() - SUFFIX.size()));
	}
};
}
