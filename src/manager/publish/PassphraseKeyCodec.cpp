#include "manager/publish/PassphraseKeyCodec.h"

#include "domain/rules/PublisherNameRule.h"
#include "manager/trust/SodiumRuntime.h"

#include <sodium.h>

#include <algorithm>
#include <array>

namespace wgrd::manager {
namespace {
	constexpr std::array<std::uint8_t, 8> MAGIC = {'W', 'G', 'R', 'D', 'K', 'E', 'Y', '1'};

	constexpr std::size_t SALT_BYTES = crypto_pwhash_SALTBYTES;
	constexpr std::size_t NONCE_BYTES = crypto_secretbox_NONCEBYTES;
	constexpr std::size_t MAC_BYTES = crypto_secretbox_MACBYTES;
	constexpr std::size_t SESSION_KEY_BYTES = crypto_secretbox_KEYBYTES;
	constexpr std::size_t SECRET_KEY_BYTES = crypto_sign_SECRETKEYBYTES;

	constexpr std::size_t MAGIC_OFFSET = 0;
	constexpr std::size_t SALT_OFFSET = MAGIC_OFFSET + MAGIC.size();
	constexpr std::size_t NONCE_OFFSET = SALT_OFFSET + SALT_BYTES;
	constexpr std::size_t CIPHERTEXT_OFFSET = NONCE_OFFSET + NONCE_BYTES;

	constexpr std::size_t NAME_LENGTH_BYTES = 2;

	using SessionKey = std::array<std::uint8_t, SESSION_KEY_BYTES>;

	std::expected<SessionKey, PassphraseKeyCodecError> DeriveSessionKey(
		const std::string_view passphrase,
		const std::span<const std::uint8_t> salt
	) {
		SessionKey sessionKey{};

		const int derived = crypto_pwhash(
			sessionKey.data(),
			sessionKey.size(),
			passphrase.data(),
			passphrase.size(),
			salt.data(),
			crypto_pwhash_OPSLIMIT_MODERATE,
			crypto_pwhash_MEMLIMIT_MODERATE,
			crypto_pwhash_ALG_ARGON2ID13
		);

		if (derived != 0) {
			sodium_memzero(sessionKey.data(), sessionKey.size());
			return std::unexpected(PassphraseKeyCodecError::DerivationFailed);
		}

		return sessionKey;
	}
}

std::expected<std::vector<std::uint8_t>, PassphraseKeyCodecError> PassphraseKeyCodec::Seal(
	std::span<const std::uint8_t> secretKey,
	const std::string_view publisherName,
	const std::string_view passphrase
) {
	if (!SodiumRuntime::Ready()) {
		return std::unexpected(PassphraseKeyCodecError::DerivationFailed);
	}

	if (passphrase.size() < MINIMUM_PASSPHRASE_LENGTH) {
		return std::unexpected(PassphraseKeyCodecError::PassphraseTooShort);
	}

	if (!domain::PublisherNameRule::IsAcceptable(publisherName)) {
		return std::unexpected(PassphraseKeyCodecError::PublisherRejected);
	}

	if (secretKey.size() != SECRET_KEY_BYTES) {
		return std::unexpected(PassphraseKeyCodecError::SecretKeyRejected);
	}

	std::vector<std::uint8_t> plaintext;
	plaintext.reserve(NAME_LENGTH_BYTES + publisherName.size() + SECRET_KEY_BYTES);

	plaintext.push_back(static_cast<std::uint8_t>(publisherName.size() & 0xFF));
	plaintext.push_back(static_cast<std::uint8_t>((publisherName.size() >> 8) & 0xFF));

	for (const char character : publisherName) {
		plaintext.push_back(static_cast<std::uint8_t>(character));
	}

	plaintext.insert(plaintext.end(), secretKey.begin(), secretKey.end());

	std::array<std::uint8_t, SALT_BYTES> salt{};
	randombytes_buf(salt.data(), salt.size());

	std::array<std::uint8_t, NONCE_BYTES> nonce{};
	randombytes_buf(nonce.data(), nonce.size());

	const auto sessionKey = DeriveSessionKey(passphrase, salt);
	if (!sessionKey.has_value()) {
		sodium_memzero(plaintext.data(), plaintext.size());
		return std::unexpected(sessionKey.error());
	}

	std::vector<std::uint8_t> sealed(CIPHERTEXT_OFFSET + MAC_BYTES + plaintext.size());

	std::ranges::copy(MAGIC, sealed.begin() + MAGIC_OFFSET);
	std::ranges::copy(salt, sealed.begin() + SALT_OFFSET);
	std::ranges::copy(nonce, sealed.begin() + NONCE_OFFSET);

	SessionKey derivedKey = *sessionKey;

	const int encrypted = crypto_secretbox_easy(
		sealed.data() + CIPHERTEXT_OFFSET,
		plaintext.data(),
		plaintext.size(),
		nonce.data(),
		derivedKey.data()
	);

	sodium_memzero(derivedKey.data(), derivedKey.size());
	sodium_memzero(plaintext.data(), plaintext.size());

	if (encrypted != 0) {
		return std::unexpected(PassphraseKeyCodecError::DerivationFailed);
	}

	return sealed;
}

std::expected<PassphraseKeyCodec::OpenedKey, PassphraseKeyCodecError> PassphraseKeyCodec::Open(
	std::span<const std::uint8_t> sealed,
	const std::string_view passphrase
) {
	if (!SodiumRuntime::Ready()) {
		return std::unexpected(PassphraseKeyCodecError::DerivationFailed);
	}

	if (sealed.size() > MAXIMUM_SEALED_BYTES) {
		return std::unexpected(PassphraseKeyCodecError::SizeRejected);
	}

	constexpr std::size_t smallest =
			CIPHERTEXT_OFFSET + MAC_BYTES + NAME_LENGTH_BYTES + SECRET_KEY_BYTES;

	if (sealed.size() < smallest) {
		return std::unexpected(PassphraseKeyCodecError::SizeRejected);
	}

	if (!std::equal(MAGIC.begin(), MAGIC.end(), sealed.begin() + MAGIC_OFFSET)) {
		return std::unexpected(PassphraseKeyCodecError::HeaderRejected);
	}

	const auto sessionKey = DeriveSessionKey(
		passphrase,
		sealed.subspan(SALT_OFFSET, SALT_BYTES)
	);

	if (!sessionKey.has_value()) {
		return std::unexpected(sessionKey.error());
	}

	const std::size_t ciphertextBytes = sealed.size() - CIPHERTEXT_OFFSET;
	std::vector<std::uint8_t> plaintext(ciphertextBytes - MAC_BYTES);

	SessionKey derivedKey = *sessionKey;

	const int opened = crypto_secretbox_open_easy(
		plaintext.data(),
		sealed.data() + CIPHERTEXT_OFFSET,
		ciphertextBytes,
		sealed.data() + NONCE_OFFSET,
		derivedKey.data()
	);

	sodium_memzero(derivedKey.data(), derivedKey.size());

	if (opened != 0) {
		sodium_memzero(plaintext.data(), plaintext.size());
		return std::unexpected(PassphraseKeyCodecError::PassphraseRejected);
	}

	const std::size_t nameLength =
			static_cast<std::size_t>(plaintext[0]) |
			(static_cast<std::size_t>(plaintext[1]) << 8);

	if (nameLength > domain::PublisherNameRule::MAXIMUM_LENGTH) {
		sodium_memzero(plaintext.data(), plaintext.size());
		return std::unexpected(PassphraseKeyCodecError::PublisherRejected);
	}

	if (plaintext.size() != NAME_LENGTH_BYTES + nameLength + SECRET_KEY_BYTES) {
		sodium_memzero(plaintext.data(), plaintext.size());
		return std::unexpected(PassphraseKeyCodecError::SizeRejected);
	}

	OpenedKey result;
	result.publisherName.assign(
		reinterpret_cast<const char*>(plaintext.data() + NAME_LENGTH_BYTES),
		nameLength
	);

	if (!domain::PublisherNameRule::IsAcceptable(result.publisherName)) {
		sodium_memzero(plaintext.data(), plaintext.size());
		return std::unexpected(PassphraseKeyCodecError::PublisherRejected);
	}

	const std::size_t secretKeyOffset = NAME_LENGTH_BYTES + nameLength;
	result.secretKey.assign(
		plaintext.begin() + static_cast<std::ptrdiff_t>(secretKeyOffset),
		plaintext.end()
	);

	sodium_memzero(plaintext.data(), plaintext.size());

	return result;
}
}
