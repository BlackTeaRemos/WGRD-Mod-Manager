#include "manager/trust/FingerprintDeriver.h"

#include <blake3.h>

#include <array>
#include <cstdint>

namespace wgrd::manager {

domain::PublisherFingerprint FingerprintDeriver::Derive(const domain::PublicKey& publicKey) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, publicKey.Data(), domain::PublicKey::BYTE_COUNT);

    std::array<std::uint8_t, BLAKE3_OUT_LEN> digest{};
    blake3_hasher_finalize(&hasher, digest.data(), digest.size());

    const auto fingerprint = domain::PublisherFingerprint::FromBytes(
        std::span<const std::uint8_t>(digest.data(), domain::PublisherFingerprint::BYTE_COUNT));

    return fingerprint.value_or(domain::PublisherFingerprint{});
}

}
