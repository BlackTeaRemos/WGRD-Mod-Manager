#include "manager/publish/SigningKeyStore.h"

#include "manager/publish/PassphraseKeyCodec.h"
#include "domain/rules/PublisherNameRule.h"
#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <array>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::manager {
namespace {
	constexpr std::size_t SECRET_KEY_BYTES = crypto_sign_SECRETKEYBYTES;
	constexpr std::size_t PUBLIC_KEY_BYTES = crypto_sign_PUBLICKEYBYTES;

	domain::SigningKeyStoreError Translate(const PassphraseKeyCodecError failure) {
		switch (failure) {
			case PassphraseKeyCodecError::PassphraseTooShort:
				return domain::SigningKeyStoreError::PassphraseTooShort;
			case PassphraseKeyCodecError::PublisherRejected:
				return domain::SigningKeyStoreError::PublisherRejected;
			case PassphraseKeyCodecError::PassphraseRejected:
				return domain::SigningKeyStoreError::PassphraseRejected;
			case PassphraseKeyCodecError::HeaderRejected:
			case PassphraseKeyCodecError::SizeRejected:
			case PassphraseKeyCodecError::SecretKeyRejected:
				return domain::SigningKeyStoreError::Corrupt;
			case PassphraseKeyCodecError::DerivationFailed:
				return domain::SigningKeyStoreError::GenerationFailed;
		}

		return domain::SigningKeyStoreError::Corrupt;
	}
}

SigningKeyStore::SigningKeyStore()
	: _secretKey()
	, _publisherName() {}

SigningKeyStore::~SigningKeyStore() {
	Lock();
}

bool SigningKeyStore::Unlocked() const {
	return _secretKey.size() == SECRET_KEY_BYTES;
}

void SigningKeyStore::Lock() {
	if (!_secretKey.empty()) {
		sodium_memzero(_secretKey.data(), _secretKey.size());
	}

	_secretKey.clear();
	_publisherName.clear();
}

void SigningKeyStore::Adopt_(std::vector<std::uint8_t> secretKey, std::string publisherName) {
	Lock();

	_secretKey = std::move(secretKey);
	_publisherName = std::move(publisherName);
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::Create(
	const std::filesystem::path& keyPath,
	const std::string_view publisherName,
	const std::string_view passphrase
) {
	if (!SodiumRuntime::Ready()) {
		return std::unexpected(domain::SigningKeyStoreError::GenerationFailed);
	}

	if (!domain::PublisherNameRule::IsAcceptable(publisherName)) {
		return std::unexpected(domain::SigningKeyStoreError::PublisherRejected);
	}

	if (passphrase.size() < PassphraseKeyCodec::MINIMUM_PASSPHRASE_LENGTH) {
		return std::unexpected(domain::SigningKeyStoreError::PassphraseTooShort);
	}

	std::error_code failure;
	if (std::filesystem::exists(keyPath, failure)) {
		return std::unexpected(domain::SigningKeyStoreError::AlreadyPresent);
	}

	std::array<std::uint8_t, PUBLIC_KEY_BYTES> publicKeyBytes{};
	std::array<std::uint8_t, SECRET_KEY_BYTES> secretKeyBytes{};

	if (crypto_sign_keypair(publicKeyBytes.data(), secretKeyBytes.data()) != 0) {
		sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
		return std::unexpected(domain::SigningKeyStoreError::GenerationFailed);
	}

	const auto sealed = PassphraseKeyCodec::Seal(secretKeyBytes, publisherName, passphrase);
	if (!sealed.has_value()) {
		sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
		return std::unexpected(Translate(sealed.error()));
	}

	const auto written = WriteSealed_(keyPath, *sealed);
	if (!written.has_value()) {
		sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
		return std::unexpected(written.error());
	}

	const auto identity = DeriveIdentity_(secretKeyBytes);
	if (!identity.has_value()) {
		sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());
		return std::unexpected(identity.error());
	}

	Adopt_(
		std::vector<std::uint8_t>(secretKeyBytes.begin(), secretKeyBytes.end()),
		std::string(publisherName)
	);

	sodium_memzero(secretKeyBytes.data(), secretKeyBytes.size());

	return *identity;
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::Unlock(
	const std::filesystem::path& keyPath,
	const std::string_view passphrase
) {
	const auto sealed = ReadSealed_(keyPath);
	if (!sealed.has_value()) {
		return std::unexpected(sealed.error());
	}

	auto opened = PassphraseKeyCodec::Open(*sealed, passphrase);
	if (!opened.has_value()) {
		return std::unexpected(Translate(opened.error()));
	}

	const auto identity = DeriveIdentity_(opened->secretKey);
	if (!identity.has_value()) {
		sodium_memzero(opened->secretKey.data(), opened->secretKey.size());
		return std::unexpected(identity.error());
	}

	Adopt_(std::move(opened->secretKey), std::move(opened->publisherName));

	return *identity;
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::Identity() const {
	if (!Unlocked()) {
		return std::unexpected(domain::SigningKeyStoreError::Locked);
	}

	return DeriveIdentity_(_secretKey);
}

const std::string& SigningKeyStore::PublisherName() const {
	return _publisherName;
}

std::expected<domain::Signature, domain::SigningKeyStoreError> SigningKeyStore::Sign(
	const std::span<const std::uint8_t> payload
) const {
	if (!SodiumRuntime::Ready()) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	if (!Unlocked()) {
		return std::unexpected(domain::SigningKeyStoreError::Locked);
	}

	std::array<std::uint8_t, crypto_sign_BYTES> signatureBytes{};
	const int signingResult = crypto_sign_detached(
		signatureBytes.data(),
		nullptr,
		payload.data(),
		payload.size(),
		_secretKey.data()
	);

	if (signingResult != 0) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	const auto signature = domain::Signature::FromBytes(signatureBytes);
	if (!signature.has_value()) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	return *signature;
}

std::expected<std::vector<std::uint8_t>, domain::SigningKeyStoreError> SigningKeyStore::ReadSealed_(
	const std::filesystem::path& keyPath
) {
	std::error_code failure;
	if (!std::filesystem::is_regular_file(keyPath, failure)) {
		return std::unexpected(domain::SigningKeyStoreError::Absent);
	}

	if (std::filesystem::file_size(keyPath, failure) > PassphraseKeyCodec::MAXIMUM_SEALED_BYTES) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	std::ifstream input(keyPath, std::ios::binary);
	if (!input) {
		return std::unexpected(domain::SigningKeyStoreError::Unreadable);
	}

	const std::istreambuf_iterator<char> first(input);
	constexpr std::istreambuf_iterator<char> last;
	const std::string raw(first, last);

	if (raw.empty()) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	std::vector<std::uint8_t> sealed;
	sealed.reserve(raw.size());
	for (const char character : raw) {
		sealed.push_back(static_cast<std::uint8_t>(character));
	}

	return sealed;
}

std::expected<void, domain::SigningKeyStoreError> SigningKeyStore::WriteSealed_(
	const std::filesystem::path& keyPath,
	const std::span<const std::uint8_t> sealed
) {
	std::error_code failure;
	if (keyPath.has_parent_path()) {
		std::filesystem::create_directories(keyPath.parent_path(), failure);
	}

	std::ofstream output(keyPath, std::ios::binary | std::ios::trunc);
	if (!output) {
		return std::unexpected(domain::SigningKeyStoreError::Unwritable);
	}

	output.write(
		reinterpret_cast<const char*>(sealed.data()),
		static_cast<std::streamsize>(sealed.size())
	);

	if (!output) {
		return std::unexpected(domain::SigningKeyStoreError::Unwritable);
	}

	return {};
}

std::expected<domain::PublisherIdentity, domain::SigningKeyStoreError> SigningKeyStore::DeriveIdentity_(
	const std::span<const std::uint8_t> secretKey
) {
	if (secretKey.size() != SECRET_KEY_BYTES) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	std::array<std::uint8_t, PUBLIC_KEY_BYTES> publicKeyBytes{};
	if (crypto_sign_ed25519_sk_to_pk(publicKeyBytes.data(), secretKey.data()) != 0) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	const auto publicKey = domain::PublicKey::FromBytes(publicKeyBytes);
	if (!publicKey.has_value()) {
		return std::unexpected(domain::SigningKeyStoreError::Corrupt);
	}

	return domain::PublisherIdentity{FingerprintDeriver::Derive(*publicKey), *publicKey};
}
}
