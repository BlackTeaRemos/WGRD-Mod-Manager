#pragma once

#include "domain/interfaces/order/OrderIdentityError.h"
#include "domain/types/content/ChunkDigest.h"

#include <expected>

namespace wgrd::domain {
class IOrderIdentity {
public:
	virtual ~IOrderIdentity() = 0;
	[[nodiscard]] virtual std::expected<ChunkDigest, OrderIdentityError> ReadHash() const = 0;
};

inline IOrderIdentity::~IOrderIdentity() = default;
}
