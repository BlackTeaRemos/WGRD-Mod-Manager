#pragma once

#include "domain/interfaces/trust/IKeyRegistry.h"
#include "domain/types/identity/RegisteredKey.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wgrd::manager {

class DirectoryKeyRegistry final : public domain::IKeyRegistry {
public:
    static constexpr std::string_view KEYS_FOLDER = "keys";
    static constexpr std::string_view REVOKED_FOLDER = "revoked";

    explicit DirectoryKeyRegistry(std::filesystem::path registryFolder);

    ~DirectoryKeyRegistry() override;

    std::size_t Reload();

    [[nodiscard]] std::optional<domain::RegisteredKey> Find(
        const domain::PublisherFingerprint& fingerprint) const override;

    [[nodiscard]] bool IsUsable(const domain::PublisherFingerprint& fingerprint) const override;

    [[nodiscard]] std::size_t Count() const override;

private:
    [[nodiscard]] std::vector<domain::RegisteredKey> ReadKeys_() const;

    [[nodiscard]] std::vector<domain::PublisherFingerprint> ReadRevocations_() const;

    [[nodiscard]] static std::optional<domain::RegisteredKey> ReadKeyFile_(
        const std::filesystem::path& path);

    std::filesystem::path _registryFolder;
    std::vector<domain::RegisteredKey> _keys;
};

}
