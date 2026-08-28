#include "manager/trust/ManifestEnvelope.h"

#include "domain/types/distribution/TransportLimits.h"

#include <algorithm>

namespace wgrd::manager {

std::vector<std::uint8_t> ManifestEnvelope::Encode(
    const domain::PublisherFingerprint& fingerprint,
    const domain::Signature& signature,
    std::span<const std::uint8_t> payload) {

    std::vector<std::uint8_t> envelope(HEADER_BYTES + payload.size(), 0);

    for (std::size_t position = 0; position < MAGIC.size(); ++position) {
        envelope[MAGIC_OFFSET + position] = static_cast<std::uint8_t>(MAGIC[position]);
    }

    WriteLittleEndian_(envelope, VERSION_OFFSET, VERSION);

    const std::span<const std::uint8_t> fingerprintBytes = fingerprint.Bytes();
    std::copy(fingerprintBytes.begin(), fingerprintBytes.end(), envelope.begin() + FINGERPRINT_OFFSET);

    WriteLittleEndian_(envelope, PAYLOAD_SIZE_OFFSET, static_cast<std::uint32_t>(payload.size()));

    const std::span<const std::uint8_t> signatureBytes = signature.Bytes();
    std::copy(signatureBytes.begin(), signatureBytes.end(), envelope.begin() + SIGNATURE_OFFSET);

    std::copy(payload.begin(), payload.end(), envelope.begin() + HEADER_BYTES);

    return envelope;
}

std::expected<EnvelopeView, domain::ManifestAuthenticationError> ManifestEnvelope::Decode(
    std::span<const std::uint8_t> envelope) {

    if (envelope.size() > domain::limits::MANIFEST_ENVELOPE_BYTES) {
        return std::unexpected(domain::ManifestAuthenticationError::TooLarge);
    }

    if (envelope.size() < HEADER_BYTES) {
        return std::unexpected(domain::ManifestAuthenticationError::TooShort);
    }

    for (std::size_t position = 0; position < MAGIC.size(); ++position) {
        if (envelope[MAGIC_OFFSET + position] != static_cast<std::uint8_t>(MAGIC[position])) {
            return std::unexpected(domain::ManifestAuthenticationError::BadMagic);
        }
    }

    if (ReadLittleEndian_(envelope, VERSION_OFFSET) != VERSION) {
        return std::unexpected(domain::ManifestAuthenticationError::UnsupportedVersion);
    }

    const std::uint32_t declared = ReadLittleEndian_(envelope, PAYLOAD_SIZE_OFFSET);
    if (envelope.size() - HEADER_BYTES != declared) {
        return std::unexpected(domain::ManifestAuthenticationError::LengthMismatch);
    }

    const auto fingerprint = domain::PublisherFingerprint::FromBytes(
        envelope.subspan(FINGERPRINT_OFFSET, domain::PublisherFingerprint::BYTE_COUNT));
    if (!fingerprint.has_value()) {
        return std::unexpected(domain::ManifestAuthenticationError::TooShort);
    }

    const auto signature = domain::Signature::FromBytes(
        envelope.subspan(SIGNATURE_OFFSET, domain::Signature::BYTE_COUNT));
    if (!signature.has_value()) {
        return std::unexpected(domain::ManifestAuthenticationError::TooShort);
    }

    return EnvelopeView{*fingerprint, *signature, envelope.subspan(HEADER_BYTES, declared)};
}

std::uint32_t ManifestEnvelope::ReadLittleEndian_(std::span<const std::uint8_t> source, std::size_t offset) {
    return static_cast<std::uint32_t>(source[offset]) |
           (static_cast<std::uint32_t>(source[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(source[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(source[offset + 3]) << 24);
}

void ManifestEnvelope::WriteLittleEndian_(
    std::vector<std::uint8_t>& target,
    std::size_t offset,
    std::uint32_t value) {

    target[offset] = static_cast<std::uint8_t>(value & 0xFF);
    target[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    target[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    target[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

}
