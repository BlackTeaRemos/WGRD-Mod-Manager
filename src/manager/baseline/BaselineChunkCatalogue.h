#pragma once

#include "domain/interfaces/content/IHeldChunkCatalogue.h"
#include "manager/baseline/BaselineFileEntry.h"
#include "manager/baseline/records/BaselineFileRecord.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class BaselineChunkCatalogue final : public domain::IHeldChunkCatalogue {
public:
	BaselineChunkCatalogue();

	~BaselineChunkCatalogue() override;

	[[nodiscard]] std::optional<domain::HeldChunkLocation> Locate(
		const domain::ChunkDigest& digest
	) const override;

	void Adopt(const std::filesystem::path& packRoot, const BaselineFileRecord& record);

	void Forget(std::string_view relativePath);

	void Clear();

	void DiscardChanged();

	[[nodiscard]] std::size_t ChunkCount() const;

private:
	void Forget_(std::string_view relativePath);

	mutable std::mutex _guard;
	std::map<std::string, std::vector<domain::HeldChunkLocation>> _locations;
	std::map<std::string, std::vector<std::string>, std::less<>> _digestsByFile;
	std::map<std::string, BaselineFileEntry, std::less<>> _filesByPath;
};
}
