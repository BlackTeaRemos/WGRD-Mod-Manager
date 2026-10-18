#include "manager/extensions/ExtensionCryptography.h"
#include "manager/extensions/ExtensionPackageContract.h"

#include <windows.h>
#include <bcrypt.h>

#include <limits>
#include <vector>

namespace wgrd::manager {
std::expected<std::array<std::uint8_t, 32>, domain::ExtensionPackageError> ExtensionCryptography::Hash(
    const std::span<const std::byte> bytes
) {
    if (bytes.size() > std::numeric_limits<ULONG>::max()) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return std::unexpected(domain::ExtensionPackageError::CryptographyUnavailable);
    }
    DWORD objectBytes = 0;
    DWORD returnedBytes = 0;
    const NTSTATUS measured = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &returnedBytes, 0);
    if (measured < 0 || returnedBytes != sizeof(objectBytes) || objectBytes > 1024 * 1024) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::unexpected(domain::ExtensionPackageError::CryptographyUnavailable);
    }
    std::vector<std::uint8_t> storage(objectBytes);
    BCRYPT_HASH_HANDLE hash = nullptr;
    const NTSTATUS created = BCryptCreateHash(algorithm, &hash, storage.data(), objectBytes, nullptr, 0, 0);
    std::array<std::uint8_t, 32> digest{};
    NTSTATUS completed = created;
    if (created >= 0) {
        completed = BCryptHashData(hash,
            reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),
            static_cast<ULONG>(bytes.size()), 0);
        if (completed >= 0) {
            completed = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (completed < 0) {
        return std::unexpected(domain::ExtensionPackageError::CryptographyUnavailable);
    }
    return digest;
}

std::expected<void, domain::ExtensionPackageError> ExtensionCryptography::VerifySignature(
    const std::span<const std::byte> manifest,
    const std::span<const std::byte> signature
) {
    if (signature.size() != ExtensionPackageContract::SIGNATURE_BYTES) {
        return std::unexpected(domain::ExtensionPackageError::InvalidSignature);
    }
    const auto digest = Hash(manifest);
    if (!digest.has_value()) {
        return std::unexpected(digest.error());
    }
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0) {
        return std::unexpected(domain::ExtensionPackageError::CryptographyUnavailable);
    }
    BCRYPT_KEY_HANDLE key = nullptr;
    const NTSTATUS imported = BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
        const_cast<PUCHAR>(ExtensionPackageContract::PUBLIC_KEY.data()),
        static_cast<ULONG>(ExtensionPackageContract::PUBLIC_KEY.size()), 0);
    NTSTATUS verified = imported;
    if (imported >= 0) {
        verified = BCryptVerifySignature(key, nullptr, const_cast<PUCHAR>(digest->data()),
            static_cast<ULONG>(digest->size()),
            reinterpret_cast<PUCHAR>(const_cast<std::byte*>(signature.data())),
            static_cast<ULONG>(signature.size()), 0);
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (imported < 0) {
        return std::unexpected(domain::ExtensionPackageError::CryptographyUnavailable);
    }
    if (verified < 0) {
        return std::unexpected(domain::ExtensionPackageError::InvalidSignature);
    }
    return {};
}
}
