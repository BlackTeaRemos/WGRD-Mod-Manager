#pragma once

#include "manager/update/HttpsClient.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {

enum class ContentsLookupError {
    RepositoryUnset,
    RequestFailed,
    ResponseMalformed,
    TooManyEntries
};

struct ContentsEntry {
    std::string name;
    std::string downloadUrl;
    std::uint64_t bytes;
};

class GitHubContentsSource {
public:
    static constexpr std::size_t MAX_ENTRIES = 128;
    static constexpr std::size_t MAX_ENTRY_BYTES = 4096;
    static constexpr std::string_view ACCEPT_HEADER = "application/vnd.github+json";
    static constexpr std::string_view API_HOST = "https://api.github.com";

    explicit GitHubContentsSource(const HttpsClient& client);

    [[nodiscard]] std::expected<std::vector<ContentsEntry>, ContentsLookupError> List(
        std::string_view repository,
        std::string_view folder) const;

    [[nodiscard]] static std::expected<std::vector<ContentsEntry>, ContentsLookupError> ParseListing(
        std::string_view document);

    [[nodiscard]] static std::string BuildUrl(std::string_view repository, std::string_view folder);

    [[nodiscard]] static bool IsRegistryFileName(std::string_view name);

private:
    const HttpsClient* _client;
};

}
