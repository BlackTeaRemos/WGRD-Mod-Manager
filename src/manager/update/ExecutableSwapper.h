#pragma once

#include <expected>
#include <filesystem>
#include <string_view>

namespace wgrd::manager {

enum class SwapError {
    ExecutableUnknown,
    StagedMissing,
    StagedEmpty,
    RetireFailed,
    InstallFailed,
    RelaunchFailed
};

class ExecutableSwapper {
public:
    static constexpr std::string_view STAGED_SUFFIX = ".new";
    static constexpr std::string_view RETIRED_SUFFIX = ".old";

    [[nodiscard]] static std::filesystem::path CurrentExecutable();

    [[nodiscard]] static std::filesystem::path StagedPath();

    [[nodiscard]] static std::filesystem::path RetiredPath();

    static bool DiscardRetired();

    [[nodiscard]] static std::expected<void, SwapError> Apply();

private:
    [[nodiscard]] static bool Relaunch_(const std::filesystem::path& executable);
};

}
