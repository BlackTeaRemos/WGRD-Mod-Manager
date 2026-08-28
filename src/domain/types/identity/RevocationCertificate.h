#pragma once

#include "domain/types/identity/PublisherFingerprint.h"
#include "domain/types/identity/Signature.h"

#include <string>

namespace wgrd::domain {
struct RevocationCertificate {
	PublisherFingerprint fingerprint;
	std::string revokedAt;
	std::string reason;
	Signature signature;

	bool operator==(const RevocationCertificate& other) const = default;
};
}
