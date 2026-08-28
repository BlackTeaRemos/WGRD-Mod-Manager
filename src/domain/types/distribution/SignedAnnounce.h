#pragma once

#include "domain/types/content/ChunkDigest.h"
#include "domain/types/identity/PublisherFingerprint.h"
#include "domain/types/identity/Signature.h"

#include <cstdint>
#include <string>

namespace wgrd::domain {

struct SignedAnnounce {
    PublisherFingerprint publisher;
    std::string modName;
    std::uint64_t version;
    ChunkDigest manifestDigest;
    ChunkDigest torrentInfoHash;
    Signature signature;

    [[nodiscard]] std::string TorrentName() const {
        return publisher.ToHex() + "_" + modName;
    }

    [[nodiscard]] std::string Identifier() const {
        return publisher.ToHex() + "/" + modName;
    }

    bool operator==(const SignedAnnounce& other) const = default;
};

}
