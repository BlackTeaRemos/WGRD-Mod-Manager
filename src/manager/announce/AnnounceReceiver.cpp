#include "manager/announce/AnnounceReceiver.h"

#include "manager/announce/AnnounceCodec.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <utility>

namespace wgrd::manager {

AnnounceReceiver::AnnounceReceiver(const domain::IKeyRegistry& registry)
    : _registry(&registry),
      _retained() {
}

AnnounceReceiver::~AnnounceReceiver() = default;

std::expected<domain::SignedAnnounce, domain::AnnounceRejection> AnnounceReceiver::Accept(
    std::span<const std::uint8_t> record) {

    const auto decoded = AnnounceCodec::Decode(record);
    if (!decoded.has_value()) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::Malformed);
    }

    const auto key = _registry->Find(decoded->publisher);
    if (!key.has_value()) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::UnknownPublisher);
    }

    if (key->revoked) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::RevokedPublisher);
    }

    if (!SodiumRuntime::Ready()) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::SignatureInvalid);
    }

    const std::vector<std::uint8_t> signable = AnnounceCodec::EncodeSignable(*decoded);

    const int verified = crypto_sign_verify_detached(
        decoded->signature.Data(),
        signable.data(),
        signable.size(),
        key->publicKey.Data());

    if (verified != 0) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::SignatureInvalid);
    }

    const std::string identifier = decoded->Identifier();

    const auto existing = _retained.find(identifier);
    if (existing != _retained.end() && existing->second.version >= decoded->version) {
        ++_rejected;
        return std::unexpected(domain::AnnounceRejection::NotNewer);
    }

    _retained.insert_or_assign(identifier, *decoded);

    return *decoded;
}

std::optional<domain::SignedAnnounce> AnnounceReceiver::Retained(const std::string& identifier) const {
    const auto match = _retained.find(identifier);
    if (match == _retained.end()) {
        return std::nullopt;
    }

    return match->second;
}

std::vector<domain::SignedAnnounce> AnnounceReceiver::All() const {
    std::vector<domain::SignedAnnounce> announces;
    announces.reserve(_retained.size());

    for (const auto& entry : _retained) {
        announces.push_back(entry.second);
    }

    return announces;
}

std::size_t AnnounceReceiver::RejectedCount() const {
    return _rejected;
}

std::size_t AnnounceReceiver::RetainedCount() const {
    return _retained.size();
}

}
