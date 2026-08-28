#pragma once

#include "domain/types/status/InstallProgress.h"

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::domain {

enum class InstallStartError {
    Busy,
    UnknownIdentifier,
    ManifestMissing,
    ManifestRejected,
    NothingToDo
};

class IInstallService {
public:
    virtual ~IInstallService() = 0;

    [[nodiscard]] virtual std::expected<void, InstallStartError> Start(
        std::string_view identifier) = 0;

    [[nodiscard]] virtual InstallProgress Progress() const = 0;

    virtual void Poll() = 0;

    virtual void Cancel() = 0;
};

inline IInstallService::~IInstallService() = default;

}
