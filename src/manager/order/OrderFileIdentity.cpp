#include "manager/order/OrderFileIdentity.h"

#include "manager/io/MappedFile.h"

#include <utility>

namespace wgrd::manager {
OrderFileIdentity::OrderFileIdentity(std::filesystem::path file, const domain::IContentHasher& hasher)
	: _file(std::move(file))
	, _hasher(&hasher) {}

std::expected<domain::ChunkDigest, domain::OrderIdentityError> OrderFileIdentity::ReadHash() const {
	const auto mapped = MappedFile::Open(_file);
	if (!mapped.has_value()) {
		return std::unexpected(domain::OrderIdentityError::Unreadable);
	}
	if (mapped->Size() > MAXIMUM_BYTES) {
		return std::unexpected(domain::OrderIdentityError::TooLarge);
	}
	return _hasher->Hash(mapped->Data());
}
}
