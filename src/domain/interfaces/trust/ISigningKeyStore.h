#pragma once

#include "domain/types/identity/PublisherIdentity.h"
#include "domain/types/identity/Signature.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace wgrd::domain {
enum class SigningKeyStoreError {
	AlreadyPresent
	, Absent
	, Locked
	, Unreadable
	, Unwritable
	, Corrupt
	, PassphraseRejected
	, PassphraseTooShort
	, PublisherRejected
	, GenerationFailed
};

class ISigningKeyStore {
public:
	virtual ~ISigningKeyStore() = 0;

	[[nodiscard]] virtual bool Unlocked() const = 0;

	[[nodiscard]] virtual std::expected<PublisherIdentity, SigningKeyStoreError> Create(
		const std::filesystem::path& keyPath,
		std::string_view publisherName,
		std::string_view passphrase
	) = 0;

	[[nodiscard]] virtual std::expected<PublisherIdentity, SigningKeyStoreError> Unlock(
		const std::filesystem::path& keyPath,
		std::string_view passphrase
	) = 0;

	virtual void Lock() = 0;

	[[nodiscard]] virtual std::expected<PublisherIdentity, SigningKeyStoreError> Identity() const = 0;

	[[nodiscard]] virtual const std::string& PublisherName() const = 0;

	[[nodiscard]] virtual std::expected<Signature, SigningKeyStoreError> Sign(
		std::span<const std::uint8_t> payload
	) const = 0;
};

inline ISigningKeyStore::~ISigningKeyStore() = default;
}
