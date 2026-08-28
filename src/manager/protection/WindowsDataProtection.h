#pragma once

#include "domain/interfaces/trust/IDataProtection.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::manager {

class WindowsDataProtection final : public domain::IDataProtection {
public:
    WindowsDataProtection();

    ~WindowsDataProtection() override;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, domain::DataProtectionError> Protect(
        std::span<const std::uint8_t> plaintext) const override;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, domain::DataProtectionError> Unprotect(
        std::span<const std::uint8_t> ciphertext) const override;
};

}
