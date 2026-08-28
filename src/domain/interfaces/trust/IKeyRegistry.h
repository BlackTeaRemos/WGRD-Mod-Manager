#pragma once

#include "domain/types/identity/PublisherFingerprint.h"
#include "domain/types/identity/RegisteredKey.h"

#include <optional>

namespace wgrd::domain {
class IKeyRegistry {
public:
	virtual ~IKeyRegistry() = 0;

	[[nodiscard]] virtual std::optional<RegisteredKey> Find(
		const PublisherFingerprint& fingerprint
	) const = 0;

	[[nodiscard]] virtual bool IsUsable(const PublisherFingerprint& fingerprint) const = 0;

	[[nodiscard]] virtual std::size_t Count() const = 0;

	virtual std::size_t Reload() = 0;
};

inline IKeyRegistry::~IKeyRegistry() = default;
}
