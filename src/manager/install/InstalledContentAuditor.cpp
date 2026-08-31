#include "manager/install/InstalledContentAuditor.h"

#include "manager/io/MappedFile.h"

#include <span>
#include <utility>

namespace wgrd::manager {
InstalledContentAuditor::InstalledContentAuditor(const domain::IContentHasher& hasher)
	: _hasher(&hasher) {}

InstalledContentAuditor::Audit InstalledContentAuditor::Examine(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::function<void(std::uint64_t)>& onVerified
) const {
	Audit audit;

	for (const domain::ManifestFile& file : manifest.Files()) {
		const auto mapped = MappedFile::Open(modFolder / file.path);

		const bool readable = mapped.has_value() && mapped->Size() == file.size;

		const std::span<const std::byte> content = readable
		                                           ? mapped->Data()
		                                           : std::span<const std::byte>();

		std::vector<domain::ChunkPlacement> placements;
		placements.reserve(file.chunks.size());

		std::size_t fileDamage = 0;

		for (const domain::ManifestChunk& chunk : file.chunks) {
			const bool present = readable
			                     && chunk.offset + chunk.length <= content.size()
			                     && _hasher->Hash(content.subspan(chunk.offset, chunk.length)) == chunk.digest;

			if (present) {
				audit.verifiedBytes += chunk.length;

				if (onVerified) {
					onVerified(audit.verifiedBytes);
				}
			} else {
				++fileDamage;
			}

			placements.push_back(domain::ChunkPlacement{
					chunk.digest,
					chunk.offset,
					chunk.length,
					present ? domain::ChunkSourceKind::Held : domain::ChunkSourceKind::Remote,
					present ? file.path : std::string(),
					present ? chunk.offset : 0
				}
			);
		}

		if (fileDamage == 0) {
			continue;
		}

		audit.damagedChunks += fileDamage;
		audit.damagedFiles.push_back(domain::FilePlan{file.path, file.size, std::move(placements)});
	}

	return audit;
}
}
