#pragma once

#include "domain/types/distribution/SignedAnnounce.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace wgrd::manager {

enum class AnnounceDecodeError {
    WrongLength,
    BadMagic,
    UnsupportedVersion,
    ModNameRejected
};

class AnnounceCodec {
public:
    static constexpr std::string_view MAGIC = "WGRDANNC";
    static constexpr std::uint32_t FORMAT_VERSION = 1;

    static constexpr std::size_t MAGIC_OFFSET = 0;
    static constexpr std::size_t FORMAT_OFFSET = 8;
    static constexpr std::size_t FINGERPRINT_OFFSET = 12;
    static constexpr std::size_t MOD_NAME_OFFSET = 20;
    static constexpr std::size_t MOD_NAME_BYTES = 64;
    static constexpr std::size_t VERSION_OFFSET = 84;
    static constexpr std::size_t MANIFEST_DIGEST_OFFSET = 92;
    static constexpr std::size_t TORRENT_INFO_HASH_OFFSET = 124;
    static constexpr std::size_t SIGNED_BYTES = 156;
    static constexpr std::size_t SIGNATURE_OFFSET = 156;
    static constexpr std::size_t RECORD_BYTES = 220;

    [[nodiscard]] static std::vector<std::uint8_t> EncodeSignable(const domain::SignedAnnounce& announce);

    [[nodiscard]] static std::vector<std::uint8_t> Encode(const domain::SignedAnnounce& announce);

    [[nodiscard]] static std::expected<domain::SignedAnnounce, AnnounceDecodeError> Decode(
        std::span<const std::uint8_t> record);

private:
    static void WriteLittleEndian32_(std::vector<std::uint8_t>& target, std::size_t offset, std::uint32_t value);

    static void WriteLittleEndian64_(std::vector<std::uint8_t>& target, std::size_t offset, std::uint64_t value);

    static std::uint32_t ReadLittleEndian32_(std::span<const std::uint8_t> source, std::size_t offset);

    static std::uint64_t ReadLittleEndian64_(std::span<const std::uint8_t> source, std::size_t offset);

    static bool IsAcceptableModName_(std::string_view modName);
};

}
