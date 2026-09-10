#pragma once

#include "manager/baseline/BaselineFileEntry.h"
#include "manager/baseline/records/BaselineFileRecord.h"
#include "manager/manifest/FileChunkLister.h"

namespace wgrd::manager {
class BaselineFileIndexer {
public:
	BaselineFileIndexer(const domain::IContentChunker& chunker, const domain::IContentHasher& hasher);
	[[nodiscard]] std::optional<BaselineFileRecord> Describe(const BaselineFileEntry& entry,
		const std::function<void(std::uint64_t)>& report) const;

private:
	FileChunkLister _lister;
};
}
