#include "manager/trust/DirectoryKeyRegistry.h"

#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/KeyRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::manager {

namespace {

using Json = nlohmann::json;

Json ReadDocument(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Json();
    }

    return Json::parse(input, nullptr, false);
}

}

DirectoryKeyRegistry::DirectoryKeyRegistry(std::filesystem::path registryFolder)
    : _registryFolder(std::move(registryFolder)),
      _keys() {

    Reload();
}

DirectoryKeyRegistry::~DirectoryKeyRegistry() = default;

std::optional<domain::RegisteredKey> DirectoryKeyRegistry::ReadKeyFile_(
    const std::filesystem::path& path) {

    const Json document = ReadDocument(path);
    if (document.is_discarded() || !document.is_object()) {
        return std::nullopt;
    }

    if (!document.contains("fingerprint") || !document.contains("publicKey")) {
        return std::nullopt;
    }

    const Json& fingerprintField = document.at("fingerprint");
    const Json& publicKeyField = document.at("publicKey");

    if (!fingerprintField.is_string() || !publicKeyField.is_string()) {
        return std::nullopt;
    }

    const auto fingerprint = domain::PublisherFingerprint::FromHex(fingerprintField.get<std::string>());
    const auto publicKey = domain::PublicKey::FromHex(publicKeyField.get<std::string>());

    if (!fingerprint.has_value() || !publicKey.has_value()) {
        return std::nullopt;
    }

    if (FingerprintDeriver::Derive(*publicKey) != *fingerprint) {
        return std::nullopt;
    }

    std::string publisher;
    if (document.contains("publisher") && document.at("publisher").is_string()) {
        publisher = document.at("publisher").get<std::string>();
    }

    return domain::RegisteredKey{*fingerprint, *publicKey, std::move(publisher), false};
}

std::vector<domain::RegisteredKey> DirectoryKeyRegistry::ReadKeys_() const {
    std::vector<domain::RegisteredKey> keys;

    std::error_code failure;
    const std::filesystem::path folder = _registryFolder / KEYS_FOLDER;
    if (!std::filesystem::is_directory(folder, failure)) {
        return keys;
    }

    std::filesystem::directory_iterator walker(folder, failure);
    if (failure) {
        return keys;
    }

    const std::filesystem::directory_iterator end;
    for (; walker != end; walker.increment(failure)) {
        if (failure) {
            break;
        }

        if (!walker->is_regular_file(failure) || failure) {
            continue;
        }

        if (walker->path().extension() != ".json") {
            continue;
        }

        auto key = ReadKeyFile_(walker->path());
        if (key.has_value()) {
            keys.push_back(std::move(*key));
        }
    }

    return keys;
}

std::vector<domain::PublisherFingerprint> DirectoryKeyRegistry::ReadRevocations_() const {
    std::vector<domain::PublisherFingerprint> revoked;

    std::error_code failure;
    const std::filesystem::path folder = _registryFolder / REVOKED_FOLDER;
    if (!std::filesystem::is_directory(folder, failure)) {
        return revoked;
    }

    std::filesystem::directory_iterator walker(folder, failure);
    if (failure) {
        return revoked;
    }

    const std::filesystem::directory_iterator end;
    for (; walker != end; walker.increment(failure)) {
        if (failure) {
            break;
        }

        if (!walker->is_regular_file(failure) || failure) {
            continue;
        }

        if (walker->path().extension() != ".json") {
            continue;
        }

        const Json document = ReadDocument(walker->path());
        if (document.is_discarded() || !document.is_object()) {
            continue;
        }

        if (!document.contains("fingerprint") || !document.at("fingerprint").is_string()) {
            continue;
        }

        const auto fingerprint = domain::PublisherFingerprint::FromHex(
            document.at("fingerprint").get<std::string>());

        if (fingerprint.has_value()) {
            revoked.push_back(*fingerprint);
        }
    }

    return revoked;
}

std::size_t DirectoryKeyRegistry::Reload() {
    std::vector<domain::RegisteredKey> merged = KeyRegistry::Baseline();

    for (domain::RegisteredKey& key : ReadKeys_()) {
        const auto existing = std::find_if(
            merged.begin(),
            merged.end(),
            [&key](const domain::RegisteredKey& candidate) {
                return candidate.fingerprint == key.fingerprint;
            });

        if (existing == merged.end()) {
            merged.push_back(std::move(key));
        }
    }

    const std::vector<domain::PublisherFingerprint> revoked = ReadRevocations_();
    for (domain::RegisteredKey& key : merged) {
        const bool listed = std::find(revoked.begin(), revoked.end(), key.fingerprint) != revoked.end();
        key.revoked = key.revoked || listed;
    }

    _keys = std::move(merged);

    return _keys.size();
}

std::optional<domain::RegisteredKey> DirectoryKeyRegistry::Find(
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

bool DirectoryKeyRegistry::IsUsable(const domain::PublisherFingerprint& fingerprint) const {
    const auto key = Find(fingerprint);
    return key.has_value() && !key->revoked;
}

std::size_t DirectoryKeyRegistry::Count() const {
    return _keys.size();
}

}
