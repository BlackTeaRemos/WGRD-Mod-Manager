#include "manager/announce/AnnounceCodec.h"

#include <algorithm>
#include <string>

namespace wgrd::manager {

namespace {

constexpr std::size_t MOD_NAME_LIMIT = 64;

bool IsAcceptableModNameCharacter(char character) {
    const bool lower = character >= 'a' && character <= 'z';
    const bool digit = character >= '0' && character <= '9';
    return lower || digit || character == '_' || character == '-';
}

}

bool AnnounceCodec::IsAcceptableModName_(std::string_view modName) {
    if (modName.empty() || modName.size() > MOD_NAME_LIMIT) {
        return false;
    }

    return std::all_of(modName.begin(), modName.end(), IsAcceptableModNameCharacter);
}

void AnnounceCodec::WriteLittleEndian32_(
    std::vector<std::uint8_t>& target,
    std::size_t offset,
    std::uint32_t value) {

    for (std::size_t position = 0; position < 4; ++position) {
        target[offset + position] = static_cast<std::uint8_t>((value >> (position * 8)) & 0xFF);
    }
}

void AnnounceCodec::WriteLittleEndian64_(
    std::vector<std::uint8_t>& target,
    std::size_t offset,
    std::uint64_t value) {

    for (std::size_t position = 0; position < 8; ++position) {
        target[offset + position] = static_cast<std::uint8_t>((value >> (position * 8)) & 0xFF);
    }
}

std::uint32_t AnnounceCodec::ReadLittleEndian32_(std::span<const std::uint8_t> source, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t position = 0; position < 4; ++position) {
        value |= static_cast<std::uint32_t>(source[offset + position]) << (position * 8);
    }
    return value;
}

std::uint64_t AnnounceCodec::ReadLittleEndian64_(std::span<const std::uint8_t> source, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t position = 0; position < 8; ++position) {
        value |= static_cast<std::uint64_t>(source[offset + position]) << (position * 8);
    }
    return value;
}

std::vector<std::uint8_t> AnnounceCodec::EncodeSignable(const domain::SignedAnnounce& announce) {
    std::vector<std::uint8_t> record(SIGNED_BYTES, 0);

    for (std::size_t position = 0; position < MAGIC.size(); ++position) {
        record[MAGIC_OFFSET + position] = static_cast<std::uint8_t>(MAGIC[position]);
    }

    WriteLittleEndian32_(record, FORMAT_OFFSET, FORMAT_VERSION);

    const std::span<const std::uint8_t> fingerprint = announce.publisher.Bytes();
    std::copy(fingerprint.begin(), fingerprint.end(), record.begin() + FINGERPRINT_OFFSET);

    const std::size_t nameLength = std::min(announce.modName.size(), MOD_NAME_BYTES);
    std::copy(
        announce.modName.begin(),
        announce.modName.begin() + static_cast<std::ptrdiff_t>(nameLength),
        record.begin() + MOD_NAME_OFFSET);

    WriteLittleEndian64_(record, VERSION_OFFSET, announce.version);

    const std::span<const std::uint8_t> manifestDigest = announce.manifestDigest.Bytes();
    std::copy(manifestDigest.begin(), manifestDigest.end(), record.begin() + MANIFEST_DIGEST_OFFSET);

    const std::span<const std::uint8_t> torrentInfoHash = announce.torrentInfoHash.Bytes();
    std::copy(torrentInfoHash.begin(), torrentInfoHash.end(), record.begin() + TORRENT_INFO_HASH_OFFSET);

    return record;
}

std::vector<std::uint8_t> AnnounceCodec::Encode(const domain::SignedAnnounce& announce) {
    std::vector<std::uint8_t> record = EncodeSignable(announce);
    record.resize(RECORD_BYTES, 0);

    const std::span<const std::uint8_t> signature = announce.signature.Bytes();
    std::copy(signature.begin(), signature.end(), record.begin() + SIGNATURE_OFFSET);

    return record;
}

std::expected<domain::SignedAnnounce, AnnounceDecodeError> AnnounceCodec::Decode(
    std::span<const std::uint8_t> record) {

    if (record.size() != RECORD_BYTES) {
        return std::unexpected(AnnounceDecodeError::WrongLength);
    }

    for (std::size_t position = 0; position < MAGIC.size(); ++position) {
        if (record[MAGIC_OFFSET + position] != static_cast<std::uint8_t>(MAGIC[position])) {
            return std::unexpected(AnnounceDecodeError::BadMagic);
        }
    }

    if (ReadLittleEndian32_(record, FORMAT_OFFSET) != FORMAT_VERSION) {
        return std::unexpected(AnnounceDecodeError::UnsupportedVersion);
    }

    const auto publisher = domain::PublisherFingerprint::FromBytes(
        record.subspan(FINGERPRINT_OFFSET, domain::PublisherFingerprint::BYTE_COUNT));

    const auto manifestDigest = domain::ChunkDigest::FromBytes(
        record.subspan(MANIFEST_DIGEST_OFFSET, domain::ChunkDigest::BYTE_COUNT));

    const auto torrentInfoHash = domain::ChunkDigest::FromBytes(
        record.subspan(TORRENT_INFO_HASH_OFFSET, domain::ChunkDigest::BYTE_COUNT));

    const auto signature = domain::Signature::FromBytes(
        record.subspan(SIGNATURE_OFFSET, domain::Signature::BYTE_COUNT));

    if (!publisher.has_value() || !manifestDigest.has_value() ||
        !torrentInfoHash.has_value() || !signature.has_value()) {
        return std::unexpected(AnnounceDecodeError::WrongLength);
    }

    const std::span<const std::uint8_t> nameBytes = record.subspan(MOD_NAME_OFFSET, MOD_NAME_BYTES);
    std::string modName;
    for (const std::uint8_t value : nameBytes) {
        if (value == 0) {
            break;
        }
        modName.push_back(static_cast<char>(value));
    }

    if (!IsAcceptableModName_(modName)) {
        return std::unexpected(AnnounceDecodeError::ModNameRejected);
    }

    return domain::SignedAnnounce{
        *publisher,
        std::move(modName),
        ReadLittleEndian64_(record, VERSION_OFFSET),
        *manifestDigest,
        *torrentInfoHash,
        *signature
    };
}

}
