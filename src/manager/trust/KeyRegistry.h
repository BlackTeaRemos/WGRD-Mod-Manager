#pragma once

#include "domain/interfaces/trust/IKeyRegistry.h"
#include "domain/types/identity/RegisteredKey.h"

#include <optional>
#include <vector>

namespace wgrd::manager {
class KeyRegistry final : public domain::IKeyRegistry {
public:
	KeyRegistry();

	explicit KeyRegistry(std::vector<domain::RegisteredKey> keys);

	~KeyRegistry() override;

	[[nodiscard]] std::optional<domain::RegisteredKey> Find(
		const domain::PublisherFingerprint& fingerprint
	) const override;

	[[nodiscard]] bool IsUsable(const domain::PublisherFingerprint& fingerprint) const override;

	[[nodiscard]] std::size_t Count() const override;

	std::size_t Reload() override;

	[[nodiscard]] static std::vector<domain::RegisteredKey> Baseline();

private:
	std::vector<domain::RegisteredKey> _keys;
};
}
