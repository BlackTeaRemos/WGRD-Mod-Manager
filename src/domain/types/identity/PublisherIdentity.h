#pragma once

#include "domain/types/identity/PublicKey.h"
#include "domain/types/identity/PublisherFingerprint.h"

namespace wgrd::domain {

struct PublisherIdentity {
    PublisherFingerprint fingerprint;
    PublicKey publicKey;
};

}
