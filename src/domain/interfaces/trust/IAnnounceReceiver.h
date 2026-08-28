#pragma once

#include "domain/types/distribution/SignedAnnounce.h"

#include <cstdint>
#include <expected>
#include <span>

namespace wgrd::domain {
enum class AnnounceRejection {
	Malformed
	, UnknownPublisher
	, RevokedPublisher
	, SignatureInvalid
	, NotNewer
};

class IAnnounceReceiver {
public:
	virtual ~IAnnounceReceiver() = 0;

	[[nodiscard]] virtual std::expected<SignedAnnounce, AnnounceRejection> Accept(
		std::span<const std::uint8_t> record
	) = 0;
};

inline IAnnounceReceiver::~IAnnounceReceiver() = default;
}
