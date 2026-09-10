#include "manager/baseline/worker/BaselineFileIndexer.h"

#include "manager/baseline/records/FileStampReader.h"

namespace wgrd::manager {
BaselineFileIndexer::BaselineFileIndexer(const domain::IContentChunker& chunker, const domain::IContentHasher& hasher)
	: _lister(chunker, hasher) {}

std::optional<BaselineFileRecord> BaselineFileIndexer::Describe(const BaselineFileEntry& entry,
	const std::function<void(std::uint64_t)>& report) const {
	const auto before = FileStampReader::Read(entry.file);
	const auto described = _lister.Describe(entry.file, entry.relativePath, report);
	const auto after = FileStampReader::Read(entry.file);
	if (!described.has_value() || !before.has_value() || before != after || described->size != before->size) {
		return std::nullopt;
	}
	return BaselineFileRecord{entry.relativePath, *before, described->chunks};
}
}
