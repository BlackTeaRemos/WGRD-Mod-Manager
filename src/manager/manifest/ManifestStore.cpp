#include "manager/manifest/ManifestStore.h"

#include "domain/types/content/ChunkDigest.h"
#include "domain/types/distribution/TransportLimits.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::manager {

ManifestStore::ManifestStore(std::filesystem::path folder)
    : _folder(std::move(folder)) {
}

bool ManifestStore::IsDigestHex_(std::string_view digestHex) {
    if (digestHex.size() != domain::ChunkDigest::HEX_LENGTH) {
        return false;
    }

    return std::all_of(digestHex.begin(), digestHex.end(), [](const char character) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        return digit || lower;
    });
}

std::filesystem::path ManifestStore::PathFor_(std::string_view digestHex) const {
    return _folder / (std::string(digestHex) + std::string(ENVELOPE_SUFFIX));
}

std::expected<std::filesystem::path, ManifestStoreError> ManifestStore::Save(
    std::string_view digestHex,
    std::span<const std::uint8_t> sealed) const {

    if (!IsDigestHex_(digestHex)) {
        return std::unexpected(ManifestStoreError::DigestRejected);
    }

    if (sealed.size() > domain::limits::MANIFEST_ENVELOPE_BYTES) {
        return std::unexpected(ManifestStoreError::TooLarge);
    }

    std::error_code failure;
    std::filesystem::create_directories(_folder, failure);
    if (failure) {
        return std::unexpected(ManifestStoreError::Unwritable);
    }

    const std::filesystem::path target = PathFor_(digestHex);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected(ManifestStoreError::Unwritable);
    }

    output.write(
        reinterpret_cast<const char*>(sealed.data()),
        static_cast<std::streamsize>(sealed.size()));

    if (!output) {
        return std::unexpected(ManifestStoreError::Unwritable);
    }

    return target;
}

std::expected<std::vector<std::uint8_t>, ManifestStoreError> ManifestStore::Load(
    std::string_view digestHex) const {

    if (!IsDigestHex_(digestHex)) {
        return std::unexpected(ManifestStoreError::DigestRejected);
    }

    const std::filesystem::path source = PathFor_(digestHex);

    std::error_code failure;
    if (!std::filesystem::is_regular_file(source, failure)) {
        return std::unexpected(ManifestStoreError::NotHeld);
    }

    const std::uintmax_t size = std::filesystem::file_size(source, failure);
    if (failure) {
        return std::unexpected(ManifestStoreError::Unreadable);
    }

    if (size > domain::limits::MANIFEST_ENVELOPE_BYTES) {
        return std::unexpected(ManifestStoreError::TooLarge);
    }

    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return std::unexpected(ManifestStoreError::Unreadable);
    }

    std::vector<std::uint8_t> sealed(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));

    if (input.gcount() != static_cast<std::streamsize>(sealed.size())) {
        return std::unexpected(ManifestStoreError::Unreadable);
    }

    return sealed;
}

std::filesystem::path ManifestStore::PathFor(std::string_view digestHex) const {
    return PathFor_(digestHex);
}

bool ManifestStore::Holds(std::string_view digestHex) const {
    if (!IsDigestHex_(digestHex)) {
        return false;
    }

    std::error_code failure;
    return std::filesystem::is_regular_file(PathFor_(digestHex), failure);
}

}
