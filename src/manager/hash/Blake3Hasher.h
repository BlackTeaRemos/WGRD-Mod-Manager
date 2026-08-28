#pragma once

#include "domain/interfaces/content/IContentHasher.h"

#include <cstddef>
#include <span>

namespace wgrd::manager {

class Blake3Hasher final : public domain::IContentHasher {
public:
    Blake3Hasher();

    ~Blake3Hasher() override;

    [[nodiscard]] domain::ChunkDigest Hash(std::span<const std::byte> data) const override;
};

}
