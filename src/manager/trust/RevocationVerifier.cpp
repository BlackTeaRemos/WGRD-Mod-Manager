#include "manager/trust/RevocationVerifier.h"

#include "manager/trust/RevocationSignable.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <cstdint>
#include <vector>

namespace wgrd::manager {
bool RevocationVerifier::Accepts(
	const domain::RevocationCertificate& certificate,
	const domain::PublicKey& publicKey
) {
	if (!SodiumRuntime::Ready()) {
		return false;
	}

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		certificate.fingerprint.ToHex(),
		certificate.revokedAt,
		certificate.reason
	);

	const int verified = crypto_sign_verify_detached(
		certificate.signature.Data(),
		signable.data(),
		signable.size(),
		publicKey.Data()
	);

	return verified == 0;
}
}
