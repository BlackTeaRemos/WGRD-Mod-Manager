#pragma once

#include "domain/types/identity/PublicKey.h"
#include "domain/types/identity/PublisherFingerprint.h"

namespace wgrd::manager {
class FingerprintDeriver {
public:
	[[nodiscard]] static domain::PublisherFingerprint Derive(const domain::PublicKey& publicKey);
};
}
