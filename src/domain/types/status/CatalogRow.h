#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
struct CatalogRow {
	std::string identifier;
	std::string modName;
	std::string publisher;
	std::uint64_t version;
	std::uint64_t totalBytes;
	std::size_t chunkCount;
	std::size_t fileCount;
	bool installed;
	bool manifestHeld;
	std::uint64_t installedVersion = 0;
	bool revoked = false;
	std::string seedFault{};

	[[nodiscard]] bool Outdated() const {
		return installed && installedVersion != 0 && version > installedVersion;
	}

	[[nodiscard]] bool VersionKnown() const {
		return installed && installedVersion != 0;
	}

	bool operator==(const CatalogRow& other) const = default;
};
}
