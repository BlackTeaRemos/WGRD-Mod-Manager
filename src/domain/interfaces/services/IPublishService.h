#pragma once

#include "domain/types/status/PublishedRelease.h"
#include "domain/types/status/PublisherState.h"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::domain {

enum class PublishError {
    KeyAlreadyPresent,
    KeyCreateFailed,
    KeyMissing,
    PublisherNameRejected,
    FolderRejected,
    ManifestBuildFailed,
    SignFailed,
    AnnounceFailed,
    StoreFailed
};

class IPublishService {
public:
    virtual ~IPublishService() = 0;

    [[nodiscard]] virtual const PublisherState& Publisher() const = 0;

    [[nodiscard]] virtual std::expected<PublisherState, PublishError> CreateKey(
        std::string_view publisherName) = 0;

    [[nodiscard]] virtual const std::vector<std::string>& Candidates() const = 0;

    virtual void RefreshCandidates() = 0;

    [[nodiscard]] virtual std::expected<PublishedRelease, PublishError> Publish(
        std::string_view folder) = 0;

    [[nodiscard]] virtual const std::vector<PublishedRelease>& History() const = 0;

    [[nodiscard]] virtual const std::string& LastMessage() const = 0;
};

inline IPublishService::~IPublishService() = default;

}
