#include "manager/manifest/ManifestBuilder.h"

#include "domain/rules/ModNameRule.h"
#include "manager/scan/ModMetadataReader.h"
#include "manager/preview/PreviewImageReader.h"
#include "domain/types/distribution/TransportLimits.h"
#include <algorithm>
#include <optional>
#include <system_error>
#include <utility>

namespace wgrd::manager {
ManifestBuilder::ManifestBuilder(
	const domain::IContentChunker& chunker,
	const domain::IContentHasher& hasher,
	const domain::IPayloadPathPolicy& pathPolicy
)
	: _lister(chunker, hasher)
	, _pathPolicy(&pathPolicy) {}

ManifestBuilder::~ManifestBuilder() = default;

bool ManifestBuilder::IsAcceptableModName_(const std::string_view modName) {
	return domain::ModNameRule::IsAcceptable(modName);
}

std::expected<std::vector<std::string>, domain::ManifestBuildError> ManifestBuilder::CollectPaths_(
	const std::filesystem::path& modFolder
) const {
	std::vector<std::string> paths;

	std::error_code failure;
	std::filesystem::recursive_directory_iterator walker(modFolder, failure);
	if (failure) {
		return std::unexpected(domain::ManifestBuildError::FolderUnreadable);
	}

	const std::filesystem::recursive_directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure) {
			return std::unexpected(domain::ManifestBuildError::FolderUnreadable);
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		const std::filesystem::path relative = std::filesystem::relative(walker->path(), modFolder, failure);
		if (failure) {
			return std::unexpected(domain::ManifestBuildError::FolderUnreadable);
		}

		const auto normalised = _pathPolicy->Normalise(relative.generic_string());
		if (relative == "thumbnail.png" || relative == "preview.png") {
			continue;
		}
		if (!normalised.has_value()) {
			return std::unexpected(domain::ManifestBuildError::PathRejected);
		}

		paths.push_back(*normalised);
	}

	std::ranges::sort(paths);

	return paths;
}

std::expected<domain::ManifestFile, domain::ManifestBuildError> ManifestBuilder::DescribeFile_(
	const std::filesystem::path& modFolder,
	const std::string& relativePath
) const {
	std::optional<domain::ManifestFile> described = _lister.Describe(modFolder / relativePath, relativePath);
	if (!described.has_value()) {
		return std::unexpected(domain::ManifestBuildError::FileUnreadable);
	}

	return std::move(*described);
}

std::uint64_t ManifestBuilder::TotalBytes_(
	const std::filesystem::path& modFolder,
	const std::vector<std::string>& paths
) {
	std::uint64_t total = 0;

	for (const std::string& relativePath : paths) {
		std::error_code failure;
		const std::uintmax_t size = std::filesystem::file_size(modFolder / relativePath, failure);

		if (!failure) {
			total += static_cast<std::uint64_t>(size);
		}
	}

	return total;
}

std::expected<domain::ModManifest, domain::ManifestBuildError> ManifestBuilder::Build(
	const std::filesystem::path& modFolder,
	const domain::PublisherFingerprint& publisher,
	const std::string_view modName,
	const std::uint64_t version
) const {
	return BuildObserved(modFolder, publisher, modName, version, {});
}

std::expected<domain::ModManifest, domain::ManifestBuildError> ManifestBuilder::BuildObserved(
	const std::filesystem::path& modFolder,
	const domain::PublisherFingerprint& publisher,
	const std::string_view modName,
	const std::uint64_t version,
	const ProgressSink& progress
) const {
	if (!IsAcceptableModName_(modName)) {
		return std::unexpected(domain::ManifestBuildError::ModNameRejected);
	}

	std::error_code failure;
	if (!std::filesystem::is_directory(modFolder, failure)) {
		return std::unexpected(domain::ManifestBuildError::FolderMissing);
	}

	const auto paths = CollectPaths_(modFolder);
	if (!paths.has_value()) {
		return std::unexpected(paths.error());
	}

	if (paths->empty()) {
		return std::unexpected(domain::ManifestBuildError::FolderEmpty);
	}

	std::vector<domain::ManifestFile> files;
	files.reserve(paths->size());

	const std::uint64_t totalBytes = progress ? TotalBytes_(modFolder, *paths) : 0;
	std::uint64_t processedBytes = 0;

	if (progress) {
		progress(0, totalBytes);
	}

	std::size_t chunkTotal = 0;

	for (const std::string& relativePath : *paths) {
		auto described = DescribeFile_(modFolder, relativePath);
		if (!described.has_value()) {
			return std::unexpected(described.error());
		}

		chunkTotal += described->chunks.size();
		if (chunkTotal > domain::limits::MANIFEST_CHUNK_COUNT) {
			return std::unexpected(domain::ManifestBuildError::TooManyChunks);
		}

		processedBytes += described->size;

		if (progress) {
			progress(processedBytes, totalBytes);
		}

		files.push_back(std::move(*described));
	}

	auto preview = PreviewImageReader::Read(modFolder);
	if (!preview.has_value()) {
		return std::unexpected(domain::ManifestBuildError::FileUnreadable);
	}
	std::shared_ptr<const domain::ModPreview> images;
	if (!preview->thumbnail.empty() || !preview->image.empty()) {
		images = std::make_shared<domain::ModPreview>(std::move(*preview));
	}
	return domain::ModManifest(publisher, std::string(modName), version, std::move(files),
		ModMetadataReader::Read(modFolder).description, std::move(images));
}
}
