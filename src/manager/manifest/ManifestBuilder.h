#pragma once

#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/content/IManifestBuilder.h"
#include "domain/interfaces/content/IPayloadPathPolicy.h"
#include "domain/types/content/ManifestFile.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class ManifestBuilder final : public domain::IManifestBuilder {
public:
	using ProgressSink = std::function<void(std::uint64_t processedBytes, std::uint64_t totalBytes)>;

	ManifestBuilder(
		const domain::IContentChunker& chunker,
		const domain::IContentHasher& hasher,
		const domain::IPayloadPathPolicy& pathPolicy
	);

	~ManifestBuilder() override;

	[[nodiscard]] std::expected<domain::ModManifest, domain::ManifestBuildError> Build(
		const std::filesystem::path& modFolder,
		const domain::PublisherFingerprint& publisher,
		std::string_view modName,
		std::uint64_t version
	) const override;

	[[nodiscard]] std::expected<domain::ModManifest, domain::ManifestBuildError> BuildObserved(
		const std::filesystem::path& modFolder,
		const domain::PublisherFingerprint& publisher,
		std::string_view modName,
		std::uint64_t version,
		const ProgressSink& progress
	) const;

private:
	[[nodiscard]] std::expected<std::vector<std::string>, domain::ManifestBuildError> CollectPaths_(
		const std::filesystem::path& modFolder
	) const;

	[[nodiscard]] std::expected<domain::ManifestFile, domain::ManifestBuildError> DescribeFile_(
		const std::filesystem::path& modFolder,
		const std::string& relativePath
	) const;

	[[nodiscard]] static std::uint64_t TotalBytes_(
		const std::filesystem::path& modFolder,
		const std::vector<std::string>& paths
	);

	static bool IsAcceptableModName_(std::string_view modName);

	const domain::IContentChunker* _chunker;
	const domain::IContentHasher* _hasher;
	const domain::IPayloadPathPolicy* _pathPolicy;
};
}
