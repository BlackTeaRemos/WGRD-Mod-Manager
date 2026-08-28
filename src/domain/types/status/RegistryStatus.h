#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wgrd::domain {

enum class RegistryPhase {
    Never,
    Polling,
    Fresh,
    Failed
};

struct RegistryStatus {
    RegistryPhase phase = RegistryPhase::Never;
    std::size_t keyCount = 0;
    std::size_t revokedCount = 0;
    std::uint64_t secondsSincePoll = 0;
    std::string message;

    [[nodiscard]] bool Busy() const {
        return phase == RegistryPhase::Polling;
    }
};

}
