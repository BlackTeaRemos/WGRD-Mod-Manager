#include "manager/manifest/ManifestBuilder.h"

#include "domain/types/distribution/TransportLimits.h"
#include "manager/io/MappedFile.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace wgrd::manager {

namespace {

constexpr std::size_t MOD_NAME_LIMIT = 64;

bool IsAcceptableModNameCharacter(char character) {
    const bool lower = character >= 'a' && character <= 'z';
    const bool digit = character >= '0' && character <= '9';
    return lower || digit || character == '_' || character == '-';
}

}

ManifestBuilder::ManifestBuilder(
    const domain::IContentChunker& chunker,
    const domain::IContentHasher& hasher,
    const domain::IPayloadPathPolicy& pathPolicy)
    : _chunker(&chunker),
      _hasher(&hasher),
      _pathPolicy(&pathPolicy) {
}

ManifestBuilder::~ManifestBuilder() = default;

bool ManifestBuilder::IsAcceptableModName_(std::string_view modName) {
    if (modName.empty() || modName.size() > MOD_NAME_LIMIT) {
        return false;
    }

    return std::all_of(modName.begin(), modName.end(), IsAcceptableModNameCharacter);
}

std::expected<std::vector<std::string>, domain::ManifestBuildError> ManifestBuilder::CollectPaths_(
    const std::filesystem::path& modFolder) const {

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
        if (!normalised.has_value()) {
            return std::unexpected(domain::ManifestBuildError::PathRejected);
        }

        paths.push_back(*normalised);
    }

    std::sort(paths.begin(), paths.end());

    return paths;
}

std::expected<domain::ManifestFile, domain::ManifestBuildError> ManifestBuilder::DescribeFile_(
    const std::filesystem::path& modFolder,
    const std::string& relativePath) const {

    const auto mapped = MappedFile::Open(modFolder / relativePath);
    if (!mapped.has_value()) {
        return std::unexpected(domain::ManifestBuildError::FileUnreadable);
    }

    const std::span<const std::byte> content = mapped->Data();
    const std::vector<domain::ChunkSpan> spans = _chunker->Split(content);

    std::vector<domain::ManifestChunk> chunks;
    chunks.reserve(spans.size());

    for (const domain::ChunkSpan& span : spans) {
        const std::span<const std::byte> slice = content.subspan(span.offset, span.length);

        chunks.push_back(domain::ManifestChunk{
            _hasher->Hash(slice),
            static_cast<std::uint64_t>(span.offset),
            static_cast<std::uint32_t>(span.length)
        });
    }

    return domain::ManifestFile{
        relativePath,
        static_cast<std::uint64_t>(mapped->Size()),
        std::move(chunks)
    };
}

std::expected<domain::ModManifest, domain::ManifestBuildError> ManifestBuilder::Build(
    const std::filesystem::path& modFolder,
    const domain::PublisherFingerprint& publisher,
    std::string_view modName,
    std::uint64_t version) const {

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

        files.push_back(std::move(*described));
    }

    return domain::ModManifest(publisher, std::string(modName), version, std::move(files));
}

}
