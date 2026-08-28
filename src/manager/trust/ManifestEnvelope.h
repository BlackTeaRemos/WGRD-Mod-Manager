#pragma once

#include "domain/interfaces/trust/IManifestAuthenticator.h"
#include "domain/types/identity/PublisherFingerprint.h"
#include "domain/types/identity/Signature.h"

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace wgrd::manager {
struct EnvelopeView {
	domain::PublisherFingerprint fingerprint;
	domain::Signature signature;
	std::span<const std::uint8_t> payload;
};

class ManifestEnvelope {
public:
	static constexpr std::string_view MAGIC = "WGRDMANI";
	static constexpr std::uint32_t VERSION = 1;

	static constexpr std::size_t MAGIC_OFFSET = 0;
	static constexpr std::size_t VERSION_OFFSET = 8;
	static constexpr std::size_t FINGERPRINT_OFFSET = 12;
	static constexpr std::size_t PAYLOAD_SIZE_OFFSET = 20;
	static constexpr std::size_t SIGNATURE_OFFSET = 24;
	static constexpr std::size_t HEADER_BYTES = 88;

	static std::vector<std::uint8_t> Encode(
		const domain::PublisherFingerprint& fingerprint,
		const domain::Signature& signature,
		std::span<const std::uint8_t> payload
	);

	static std::expected<EnvelopeView, domain::ManifestAuthenticationError> Decode(
		std::span<const std::uint8_t> envelope
	);

private:
	static std::uint32_t ReadLittleEndian_(std::span<const std::uint8_t> source, std::size_t offset);

	static void WriteLittleEndian_(std::vector<std::uint8_t>& target, std::size_t offset, std::uint32_t value);
};
}
