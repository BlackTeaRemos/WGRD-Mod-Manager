#pragma once

#include "domain/interfaces/order/IOrderIdentity.h"
#include "domain/interfaces/content/IContentHasher.h"

#include <filesystem>

namespace wgrd::manager {
class OrderFileIdentity final : public domain::IOrderIdentity {
public:
	OrderFileIdentity(std::filesystem::path file, const domain::IContentHasher& hasher);
	~OrderFileIdentity() override = default;
	[[nodiscard]] std::expected<domain::ChunkDigest, domain::OrderIdentityError> ReadHash() const override;

private:
	static constexpr std::size_t MAXIMUM_BYTES = 1024u * 1024u;
	std::filesystem::path _file;
	const domain::IContentHasher* _hasher;
};
}
