#include "manager/trust/KeyRegistry.h"

#include "domain/RegistryBaseline.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {

KeyRegistry::KeyRegistry()
    : KeyRegistry(Baseline()) {
}

KeyRegistry::KeyRegistry(std::vector<domain::RegisteredKey> keys)
    : _keys(std::move(keys)) {
}

KeyRegistry::~KeyRegistry() = default;

std::vector<domain::RegisteredKey> KeyRegistry::Baseline() {
    std::vector<domain::RegisteredKey> keys;
    keys.reserve(domain::registry::KEY_COUNT);

    for (const domain::registry::BaselineKey& entry : domain::registry::KEYS) {
        const auto fingerprint = domain::PublisherFingerprint::FromHex(entry.fingerprint);
        const auto publicKey = domain::PublicKey::FromBytes(entry.publicKey);

        if (!fingerprint.has_value() || !publicKey.has_value()) {
            continue;
        }

        keys.push_back(domain::RegisteredKey{
            *fingerprint,
            *publicKey,
            std::string(entry.publisher),
            entry.revoked
        });
    }

    return keys;
}

std::optional<domain::RegisteredKey> KeyRegistry::Find(
    const domain::PublisherFingerprint& fingerprint) const {

    const auto match = std::find_if(
        _keys.begin(),
        _keys.end(),
        [&fingerprint](const domain::RegisteredKey& key) {
            return key.fingerprint == fingerprint;
        });

    if (match == _keys.end()) {
        return std::nullopt;
    }

    return *match;
}

bool KeyRegistry::IsUsable(const domain::PublisherFingerprint& fingerprint) const {
    const auto key = Find(fingerprint);
    return key.has_value() && !key->revoked;
}

std::size_t KeyRegistry::Count() const {
    return _keys.size();
}

}
