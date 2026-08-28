#pragma once

#include "domain/interfaces/trust/IKeyRegistry.h"
#include "domain/interfaces/trust/IManifestAuthenticator.h"

#include <cstdint>
#include <expected>
#include <span>

namespace wgrd::manager {

class ManifestAuthenticator final : public domain::IManifestAuthenticator {
public:
    explicit ManifestAuthenticator(const domain::IKeyRegistry& registry);

    ~ManifestAuthenticator() override;

    [[nodiscard]] std::expected<domain::AuthenticatedManifest, domain::ManifestAuthenticationError>
    Authenticate(std::span<const std::uint8_t> envelope) const override;

private:
    const domain::IKeyRegistry* _registry;
};

}
