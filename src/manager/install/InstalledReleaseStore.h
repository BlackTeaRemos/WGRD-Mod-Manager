#pragma once

#include "domain/types/content/InstalledRelease.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class InstalledReleaseStore {
public:
	static constexpr std::string_view FILE_EXTENSION = ".wgrdi";
	static constexpr std::size_t RECORD_LIMIT = 4096;

	explicit InstalledReleaseStore(std::filesystem::path folder);

	~InstalledReleaseStore();

	[[nodiscard]] std::optional<domain::InstalledRelease> Find(std::string_view identifier) const;

	[[nodiscard]] std::vector<domain::InstalledRelease> LoadAll() const;

	[[nodiscard]] bool Save(const domain::InstalledRelease& release) const;

	[[nodiscard]] bool Remove(std::string_view identifier) const;

private:
	[[nodiscard]] static std::string FileStemFor_(std::string_view identifier);

	[[nodiscard]] std::filesystem::path PathFor_(std::string_view identifier) const;

	std::filesystem::path _folder;
};
}
