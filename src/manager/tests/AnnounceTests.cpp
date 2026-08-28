#include "manager/announce/AnnounceCodec.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/publish/AnnounceSigner.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/KeyRegistry.h"

#include "domain/types/distribution/TransportLimits.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

using wgrd::domain::AnnounceRejection;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::RegisteredKey;
using wgrd::domain::SignedAnnounce;
using wgrd::manager::AnnounceCodec;
using wgrd::manager::AnnounceDecodeError;
using wgrd::manager::AnnounceReceiver;
using wgrd::manager::AnnounceSigner;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::KeyRegistry;
using wgrd::manager::SigningKeyStore;

namespace {
class TemporaryKeyPath {
public:
	explicit TemporaryKeyPath(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-announce" / label / "publisher.key";

		std::error_code failure;
		std::filesystem::remove_all(_path.parent_path(), failure);
	}

	~TemporaryKeyPath() {
		std::error_code failure;
		std::filesystem::remove_all(_path.parent_path(), failure);
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};

wgrd::domain::ChunkDigest TestInfoHash() {
	const auto digest = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'e'));
	REQUIRE(digest.has_value());
	return *digest;
}

ModManifest MakeManifest(const wgrd::domain::PublisherFingerprint& publisher, const std::uint64_t version) {
	const auto digest = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'c'));
	REQUIRE(digest.has_value());

	ManifestFile file{"payload.dat", 16, {wgrd::domain::ManifestChunk{*digest, 0, 16}}};

	return ModManifest(publisher, "angel_maps", version, {file});
}
}

TEST_CASE("announce record fits the declared ceiling") {
	REQUIRE(AnnounceCodec::RECORD_BYTES <= wgrd::domain::limits::ANNOUNCE_RECORD_BYTES);
}

TEST_CASE("announce round trip preserves every field") {
	const auto publisher = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
	const auto manifestDigest = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'a'));
	const auto torrentInfoHash = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'b'));
	const auto signature = wgrd::domain::Signature::FromHex(std::string(128, 'd'));

	REQUIRE(publisher.has_value());
	REQUIRE(manifestDigest.has_value());
	REQUIRE(torrentInfoHash.has_value());
	REQUIRE(signature.has_value());

	const SignedAnnounce announce{
		*publisher, "angel_maps", 42, *manifestDigest, *torrentInfoHash, *signature
	};

	const std::vector<std::uint8_t> encoded = AnnounceCodec::Encode(announce);
	REQUIRE(encoded.size() == AnnounceCodec::RECORD_BYTES);

	const auto decoded = AnnounceCodec::Decode(encoded);

	REQUIRE(decoded.has_value());
	REQUIRE(*decoded == announce);
}

TEST_CASE("announce decode rejects a wrong length buffer") {
	const std::vector<std::uint8_t> truncated(AnnounceCodec::RECORD_BYTES - 1, 0);

	const auto decoded = AnnounceCodec::Decode(truncated);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == AnnounceDecodeError::WrongLength);
}

TEST_CASE("announce decode rejects foreign magic") {
	const auto publisher = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
	REQUIRE(publisher.has_value());

	const SignedAnnounce announce{*publisher, "mod", 1, {}, {}, {}};
	auto encoded = AnnounceCodec::Encode(announce);
	encoded[0] = static_cast<std::uint8_t>('X');

	const auto decoded = AnnounceCodec::Decode(encoded);

	REQUIRE_FALSE(decoded.has_value());
	REQUIRE(decoded.error() == AnnounceDecodeError::BadMagic);
}

TEST_CASE("receiver accepts a signed announce and rejects a rollback") {
	const TemporaryKeyPath keyPath("rollback");
	SigningKeyStore keyStore;

	const auto identity = keyStore.Create(keyPath.Value(), "tester", "correct horse battery");
	REQUIRE(identity.has_value());

	const Blake3Hasher hasher;
	const AnnounceSigner signer(keyStore, hasher);

	const std::vector<std::uint8_t> sealed{1, 2, 3, 4};

	const auto first = signer.Announce(MakeManifest(identity->fingerprint, 1), sealed, TestInfoHash());
	const auto second = signer.Announce(MakeManifest(identity->fingerprint, 2), sealed, TestInfoHash());
	REQUIRE(first.has_value());
	REQUIRE(second.has_value());

	const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", false}});
	AnnounceReceiver receiver(registry, nullptr);

	REQUIRE(receiver.Accept(AnnounceCodec::Encode(*second)).has_value());
	REQUIRE(receiver.RetainedCount() == 1);

	const auto replayed = receiver.Accept(AnnounceCodec::Encode(*first));

	REQUIRE_FALSE(replayed.has_value());
	REQUIRE(replayed.error() == AnnounceRejection::NotNewer);

	const auto retained = receiver.Retained(second->Identifier());
	REQUIRE(retained.has_value());
	REQUIRE(retained->version == 2);
}

TEST_CASE("receiver refuses a replay of the same version") {
	const TemporaryKeyPath keyPath("replay");
	SigningKeyStore keyStore;

	const auto identity = keyStore.Create(keyPath.Value(), "tester", "correct horse battery");
	REQUIRE(identity.has_value());

	const Blake3Hasher hasher;
	const AnnounceSigner signer(keyStore, hasher);
	const auto announce = signer.Announce(MakeManifest(identity->fingerprint, 5), {}, TestInfoHash());
	REQUIRE(announce.has_value());

	const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", false}});
	AnnounceReceiver receiver(registry, nullptr);

	REQUIRE(receiver.Accept(AnnounceCodec::Encode(*announce)).has_value());

	const auto again = receiver.Accept(AnnounceCodec::Encode(*announce));

	REQUIRE_FALSE(again.has_value());
	REQUIRE(again.error() == AnnounceRejection::NotNewer);
}

TEST_CASE("receiver rejects a tampered announce") {
	const TemporaryKeyPath keyPath("tampered");
	SigningKeyStore keyStore;

	const auto identity = keyStore.Create(keyPath.Value(), "tester", "correct horse battery");
	REQUIRE(identity.has_value());

	const Blake3Hasher hasher;
	const AnnounceSigner signer(keyStore, hasher);
	const auto announce = signer.Announce(MakeManifest(identity->fingerprint, 1), {}, TestInfoHash());
	REQUIRE(announce.has_value());

	auto encoded = AnnounceCodec::Encode(*announce);
	encoded[AnnounceCodec::VERSION_OFFSET] = 99;

	const KeyRegistry registry({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", false}});
	AnnounceReceiver receiver(registry, nullptr);

	const auto accepted = receiver.Accept(encoded);

	REQUIRE_FALSE(accepted.has_value());
	REQUIRE(accepted.error() == AnnounceRejection::SignatureInvalid);
}

TEST_CASE("receiver rejects an unregistered and a revoked publisher") {
	const TemporaryKeyPath keyPath("unregistered");
	SigningKeyStore keyStore;

	const auto identity = keyStore.Create(keyPath.Value(), "tester", "correct horse battery");
	REQUIRE(identity.has_value());

	const Blake3Hasher hasher;
	const AnnounceSigner signer(keyStore, hasher);
	const auto announce = signer.Announce(MakeManifest(identity->fingerprint, 1), {}, TestInfoHash());
	REQUIRE(announce.has_value());

	const std::vector<std::uint8_t> encoded = AnnounceCodec::Encode(*announce);

	const KeyRegistry empty({});
	AnnounceReceiver strangerReceiver(empty, nullptr);
	const auto stranger = strangerReceiver.Accept(encoded);

	REQUIRE_FALSE(stranger.has_value());
	REQUIRE(stranger.error() == AnnounceRejection::UnknownPublisher);

	const KeyRegistry revoked({RegisteredKey{identity->fingerprint, identity->publicKey, "tester", true}});
	AnnounceReceiver revokedReceiver(revoked, nullptr);
	const auto banned = revokedReceiver.Accept(encoded);

	REQUIRE_FALSE(banned.has_value());
	REQUIRE(banned.error() == AnnounceRejection::RevokedPublisher);
}

TEST_CASE("announce refuses a manifest from another publisher") {
	const TemporaryKeyPath keyPath("mismatch");
	SigningKeyStore keyStore;

	REQUIRE(keyStore.Create(keyPath.Value(), "tester", "correct horse battery").has_value());

	const auto stranger = wgrd::domain::PublisherFingerprint::FromHex("ffffffffffffffff");
	REQUIRE(stranger.has_value());

	const Blake3Hasher hasher;
	const AnnounceSigner signer(keyStore, hasher);

	const auto announce = signer.Announce(MakeManifest(*stranger, 1), {}, TestInfoHash());

	REQUIRE_FALSE(announce.has_value());
	REQUIRE(announce.error() == wgrd::manager::AnnounceSignError::PublisherMismatch);
}
