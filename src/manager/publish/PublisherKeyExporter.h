#pragma once

#include "domain/types/identity/PublisherIdentity.h"

#include <expected>
#include <filesystem>
#include <string_view>

namespace wgrd::manager {

enum class PublisherKeyExportError {
    AlreadyPresent,
    Unwritable,
    PublisherRejected
};

class PublisherKeyExporter {
public:
    explicit PublisherKeyExporter(std::filesystem::path registryFolder);

    [[nodiscard]] std::expected<std::filesystem::path, PublisherKeyExportError> Export(
        const domain::PublisherIdentity& identity,
        std::string_view publisher,
        std::string_view addedAt) const;

private:
    static bool IsAcceptablePublisher_(std::string_view publisher);

    std::filesystem::path _registryFolder;
};

}
