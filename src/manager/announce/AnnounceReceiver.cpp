#include "manager/announce/AnnounceReceiver.h"

#include "manager/announce/AnnounceCodec.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <string>
#include <utility>

namespace wgrd::manager {
namespace {
	constexpr std::size_t MAXIMUM_RETAINED = 4096;
}

AnnounceReceiver::AnnounceReceiver(const domain::IKeyRegistry& registry, const AnnounceStore* store)
	: _registry(&registry)
	, _store(store)
	, _guard()
	, _retained()
	, _rejected(0) {}

AnnounceReceiver::~AnnounceReceiver() = default;

std::size_t AnnounceReceiver::Restore() {
	if (_store == nullptr) {
		return 0;
	}

	std::size_t restored = 0;

	for (const std::vector<std::uint8_t>& record : _store->LoadAll()) {
		if (Accept(record).has_value()) {
			++restored;
		}
	}

	return restored;
}

std::string AnnounceReceiver::IdentifierOf_(
	const domain::PublisherFingerprint& publisher,
	const std::string_view modName
) {
	return publisher.ToHex() + "/" + std::string(modName);
}

std::expected<domain::SignedAnnounce, domain::AnnounceRejection> AnnounceReceiver::Accept(
	const std::span<const std::uint8_t> record
) {
	const std::scoped_lock lock(_guard);

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
		key->publicKey.Data()
	);

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

	if (existing == _retained.end() && _retained.size() >= MAXIMUM_RETAINED) {
		++_rejected;
		return std::unexpected(domain::AnnounceRejection::NotNewer);
	}

	_retained.insert_or_assign(identifier, *decoded);

	if (_store != nullptr) {
		const bool saved = _store->Save(*decoded, record);
		(void)saved;
	}

	return *decoded;
}

std::vector<domain::AnnounceSummary> AnnounceReceiver::Summaries() const {
	const std::scoped_lock lock(_guard);

	std::vector<domain::AnnounceSummary> summaries;
	summaries.reserve(_retained.size());

	for (const auto& entry : _retained) {
		summaries.push_back(domain::AnnounceSummary{
				entry.second.publisher, entry.second.modName, entry.second.version
			}
		);
	}

	return summaries;
}

std::optional<std::vector<std::uint8_t>> AnnounceReceiver::Record(
	const domain::PublisherFingerprint& publisher,
	const std::string_view modName
) const {
	const std::scoped_lock lock(_guard);

	const auto match = _retained.find(IdentifierOf_(publisher, modName));
	if (match == _retained.end()) {
		return std::nullopt;
	}

	return AnnounceCodec::Encode(match->second);
}

bool AnnounceReceiver::WouldAccept(
	const domain::PublisherFingerprint& publisher,
	const std::string_view modName,
	const std::uint64_t version
) const {
	const std::scoped_lock lock(_guard);

	if (!_registry->IsUsable(publisher)) {
		return false;
	}

	const auto match = _retained.find(IdentifierOf_(publisher, modName));
	if (match == _retained.end()) {
		return _retained.size() < MAXIMUM_RETAINED;
	}

	return version > match->second.version;
}

std::optional<domain::SignedAnnounce> AnnounceReceiver::Retained(const std::string& identifier) const {
	const std::scoped_lock lock(_guard);

	const auto match = _retained.find(identifier);
	if (match == _retained.end()) {
		return std::nullopt;
	}

	return match->second;
}

std::vector<domain::SignedAnnounce> AnnounceReceiver::All() const {
	const std::scoped_lock lock(_guard);

	std::vector<domain::SignedAnnounce> announces;
	announces.reserve(_retained.size());

	for (const auto& entry : _retained) {
		announces.push_back(entry.second);
	}

	return announces;
}

std::size_t AnnounceReceiver::RejectedCount() const {
	const std::scoped_lock lock(_guard);
	return _rejected;
}

std::size_t AnnounceReceiver::RetainedCount() const {
	const std::scoped_lock lock(_guard);
	return _retained.size();
}
}
