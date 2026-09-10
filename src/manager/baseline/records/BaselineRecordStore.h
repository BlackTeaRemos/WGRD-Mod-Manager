#pragma once

#include "domain/interfaces/content/IContentHasher.h"
#include "manager/baseline/records/BaselineFileRecord.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class BaselineRecordStore {
public:
	static constexpr std::string_view FILE_EXTENSION = ".wgrdb";
	static constexpr std::size_t RECORD_LIMIT = 4096;

	BaselineRecordStore(std::filesystem::path folder, const domain::IContentHasher& hasher);

	[[nodiscard]] std::vector<BaselineFileRecord> LoadAll() const;

	[[nodiscard]] bool Save(const BaselineFileRecord& record) const;

	[[nodiscard]] bool Remove(std::string_view relativePath) const;

private:
	[[nodiscard]] std::filesystem::path PathFor_(std::string_view relativePath) const;

	std::filesystem::path _folder;
	const domain::IContentHasher* _hasher;
};
}
