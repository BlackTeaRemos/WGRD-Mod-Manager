#include "manager/protection/WindowsDataProtection.h"

#include <windows.h>

#include <dpapi.h>

namespace wgrd::manager {

namespace {

std::vector<std::uint8_t> Absorb(DATA_BLOB& blob) {
    std::vector<std::uint8_t> bytes(blob.pbData, blob.pbData + blob.cbData);

    if (blob.pbData != nullptr) {
        SecureZeroMemory(blob.pbData, blob.cbData);
        LocalFree(blob.pbData);
        blob.pbData = nullptr;
    }

    return bytes;
}

DATA_BLOB Borrow(std::span<const std::uint8_t> source) {
    DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(source.size());
    blob.pbData = const_cast<BYTE*>(source.data());
    return blob;
}

}

WindowsDataProtection::WindowsDataProtection() = default;

WindowsDataProtection::~WindowsDataProtection() = default;

std::expected<std::vector<std::uint8_t>, domain::DataProtectionError> WindowsDataProtection::Protect(
    std::span<const std::uint8_t> plaintext) const {

    DATA_BLOB input = Borrow(plaintext);
    DATA_BLOB output{};

    const BOOL protectedOk = CryptProtectData(
        &input,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CRYPTPROTECT_UI_FORBIDDEN,
        &output);

    if (protectedOk == FALSE) {
        return std::unexpected(domain::DataProtectionError::ProtectFailed);
    }

    return Absorb(output);
}

std::expected<std::vector<std::uint8_t>, domain::DataProtectionError> WindowsDataProtection::Unprotect(
    std::span<const std::uint8_t> ciphertext) const {

    DATA_BLOB input = Borrow(ciphertext);
    DATA_BLOB output{};

    const BOOL unprotectedOk = CryptUnprotectData(
        &input,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CRYPTPROTECT_UI_FORBIDDEN,
        &output);

    if (unprotectedOk == FALSE) {
        return std::unexpected(domain::DataProtectionError::UnprotectFailed);
    }

    return Absorb(output);
}

}
