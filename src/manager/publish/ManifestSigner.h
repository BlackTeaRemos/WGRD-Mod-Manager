#pragma once

#include "domain/interfaces/trust/IManifestSigner.h"
#include "domain/interfaces/trust/ISigningKeyStore.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::manager {

class ManifestSigner final : public domain::IManifestSigner {
public:
    explicit ManifestSigner(const domain::ISigningKeyStore& keyStore);

    ~ManifestSigner() override;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, domain::ManifestSignerError> Seal(
        std::span<const std::uint8_t> payload) const override;

private:
    const domain::ISigningKeyStore* _keyStore;
};

}
