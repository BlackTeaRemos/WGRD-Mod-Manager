#pragma once

#include "domain/interfaces/trust/ISigningKeyStore.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class SigningKeyStore final : public domain::ISigningKeyStore {
public:
	SigningKeyStore();

	~SigningKeyStore() override;

	SigningKeyStore(const SigningKeyStore&) = delete;
	SigningKeyStore& operator=(const SigningKeyStore&) = delete;

	[[nodiscard]] bool Unlocked() const override;

	[[nodiscard]] std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> Create(
		const std::filesystem::path& keyPath,
		std::string_view publisherName,
		std::string_view passphrase
	) override;

	[[nodiscard]] std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> Unlock(
		const std::filesystem::path& keyPath,
		std::string_view passphrase
	) override;

	void Lock() override;

	[[nodiscard]] std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> Identity() const override;

	[[nodiscard]] const std::string& PublisherName() const override;

	[[nodiscard]] std::expected<domain::Signature, domain::SigningKeyStoreError> Sign(
		std::span<const std::uint8_t> payload
	) const override;

private:
	[[nodiscard]] static std::expected<std::vector<std::uint8_t>, domain::SigningKeyStoreError> ReadSealed_(
		const std::filesystem::path& keyPath
	);

	[[nodiscard]] static std::expected<void, domain::SigningKeyStoreError> WriteSealed_(
		const std::filesystem::path& keyPath,
		std::span<const std::uint8_t> sealed
	);

	[[nodiscard]] static std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> DeriveIdentity_(std::span<const std::uint8_t> secretKey);

	void Adopt_(std::vector<std::uint8_t> secretKey, std::string publisherName);

	std::vector<std::uint8_t> _secretKey;
	std::string _publisherName;
};
}
