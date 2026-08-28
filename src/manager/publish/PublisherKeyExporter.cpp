#include "manager/publish/PublisherKeyExporter.h"

#include "manager/trust/DirectoryKeyRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace wgrd::manager {

namespace {

constexpr std::size_t PUBLISHER_LIMIT = 64;

bool IsAcceptablePublisherCharacter(char character) {
    const bool upper = character >= 'A' && character <= 'Z';
    const bool lower = character >= 'a' && character <= 'z';
    const bool digit = character >= '0' && character <= '9';
    return upper || lower || digit || character == '.' || character == '_' || character == '-';
}

}

PublisherKeyExporter::PublisherKeyExporter(std::filesystem::path registryFolder)
    : _registryFolder(std::move(registryFolder)) {
}

bool PublisherKeyExporter::IsAcceptablePublisher_(std::string_view publisher) {
    if (publisher.empty() || publisher.size() > PUBLISHER_LIMIT) {
        return false;
    }

    return std::all_of(publisher.begin(), publisher.end(), IsAcceptablePublisherCharacter);
}

std::expected<std::filesystem::path, PublisherKeyExportError> PublisherKeyExporter::Export(
    const domain::PublisherIdentity& identity,
    std::string_view publisher,
    std::string_view addedAt) const {

    if (!IsAcceptablePublisher_(publisher)) {
        return std::unexpected(PublisherKeyExportError::PublisherRejected);
    }

    const std::string fingerprint = identity.fingerprint.ToHex();
    const std::filesystem::path folder = _registryFolder / DirectoryKeyRegistry::KEYS_FOLDER;
    const std::filesystem::path target = folder / (fingerprint + ".json");

    std::error_code failure;
    if (std::filesystem::exists(target, failure)) {
        return std::unexpected(PublisherKeyExportError::AlreadyPresent);
    }

    std::filesystem::create_directories(folder, failure);
    if (failure) {
        return std::unexpected(PublisherKeyExportError::Unwritable);
    }

    nlohmann::json document;
    document["fingerprint"] = fingerprint;
    document["publicKey"] = identity.publicKey.ToHex();
    document["publisher"] = std::string(publisher);
    document["addedAt"] = std::string(addedAt);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected(PublisherKeyExportError::Unwritable);
    }

    output << document.dump(2) << "\n";
    if (!output) {
        return std::unexpected(PublisherKeyExportError::Unwritable);
    }

    return target;
}

}
