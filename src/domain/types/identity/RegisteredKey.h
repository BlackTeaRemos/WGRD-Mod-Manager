#pragma once

#include "domain/types/identity/PublicKey.h"
#include "domain/types/identity/PublisherFingerprint.h"

#include <string>

namespace wgrd::domain {

struct RegisteredKey {
    PublisherFingerprint fingerprint;
    PublicKey publicKey;
    std::string publisher;
    bool revoked;
};

}
