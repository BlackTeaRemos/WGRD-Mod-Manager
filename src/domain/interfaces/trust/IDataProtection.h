#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::domain {

enum class DataProtectionError {
    ProtectFailed,
    UnprotectFailed
};

class IDataProtection {
public:
    virtual ~IDataProtection() = 0;

    [[nodiscard]] virtual std::expected<std::vector<std::uint8_t>, DataProtectionError> Protect(
        std::span<const std::uint8_t> plaintext) const = 0;

    [[nodiscard]] virtual std::expected<std::vector<std::uint8_t>, DataProtectionError> Unprotect(
        std::span<const std::uint8_t> ciphertext) const = 0;
};

inline IDataProtection::~IDataProtection() = default;

}
