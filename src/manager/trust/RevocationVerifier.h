#pragma once

#include "domain/types/identity/PublicKey.h"
#include "domain/types/identity/RevocationCertificate.h"

namespace wgrd::manager {
class RevocationVerifier {
public:
	[[nodiscard]] static bool Accepts(
		const domain::RevocationCertificate& certificate,
		const domain::PublicKey& publicKey
	);

private:
	RevocationVerifier() = delete;
};
}
