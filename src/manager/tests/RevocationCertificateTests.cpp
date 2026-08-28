#include "manager/publish/PublisherRevocationExporter.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/RevocationCertificateReader.h"
#include "manager/trust/RevocationSignable.h"
#include "manager/trust/RevocationVerifier.h"
#include "manager/trust/SodiumRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::manager::PublisherRevocationExporter;
using wgrd::manager::RevocationCertificateReader;
using wgrd::manager::RevocationSignable;
using wgrd::manager::RevocationVerifier;
using wgrd::manager::SigningKeyStore;
using wgrd::manager::SodiumRuntime;

namespace {
constexpr std::string_view PUBLISHER = "tester";
constexpr std::string_view PASSPHRASE = "correct horse battery";
constexpr std::string_view REVOKED_AT = "2026-08-29";

class TemporaryFolder {
public:
	explicit TemporaryFolder(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label;

		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
		std::filesystem::create_directories(_path, failure);
	}

	~TemporaryFolder() {
		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};

nlohmann::json ReadDocument(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	return nlohmann::json::parse(input, nullptr, false);
}
}

TEST_CASE("revocation signable joins fields with newlines") {
	const std::vector<std::uint8_t> bytes = RevocationSignable::Bytes(
		"0123456789abcdef",
		REVOKED_AT,
		"publisher retired key"
	);

	const std::string text(bytes.begin(), bytes.end());

	REQUIRE(text == "wgrd-revoke-v1\n0123456789abcdef\n2026-08-29\npublisher retired key");
}

TEST_CASE("exported revocation carries a verifiable signature") {
	const TemporaryFolder folder("revocationexport");
	SigningKeyStore store;

	const auto identity = store.Create(
		folder.Value() / "publisher.wgrdkey",
		PUBLISHER,
		PASSPHRASE
	);
	REQUIRE(identity.has_value());

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		identity->fingerprint.ToHex(),
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON
	);

	const auto signature = store.Sign(signable);
	REQUIRE(signature.has_value());

	const auto exported = PublisherRevocationExporter::Export(
		*identity,
		*signature,
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON,
		folder.Value() / "revoked",
		true
	);

	REQUIRE(exported.has_value());
	REQUIRE(exported->filename().string() == identity->fingerprint.ToHex() + ".json");

	const nlohmann::json document = ReadDocument(*exported);

	REQUIRE_FALSE(document.is_discarded());
	REQUIRE(document.at("fingerprint").get<std::string>() == identity->fingerprint.ToHex());
	REQUIRE(document.at("revokedAt").get<std::string>() == REVOKED_AT);
	REQUIRE(document.at("reason").get<std::string>() == PublisherRevocationExporter::DEFAULT_REASON);
	REQUIRE(document.at("signature").get<std::string>() == signature->ToHex());

	REQUIRE(SodiumRuntime::Ready());

	const int verified = crypto_sign_verify_detached(
		signature->Data(),
		signable.data(),
		signable.size(),
		identity->publicKey.Data()
	);

	REQUIRE(verified == 0);
}

TEST_CASE("revocation signature rejects an altered reason") {
	const TemporaryFolder folder("revocationalter");
	SigningKeyStore store;

	const auto identity = store.Create(
		folder.Value() / "publisher.wgrdkey",
		PUBLISHER,
		PASSPHRASE
	);
	REQUIRE(identity.has_value());

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		identity->fingerprint.ToHex(),
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON
	);

	const auto signature = store.Sign(signable);
	REQUIRE(signature.has_value());

	const std::vector<std::uint8_t> altered = RevocationSignable::Bytes(
		identity->fingerprint.ToHex(),
		REVOKED_AT,
		"attacker chosen reason"
	);

	REQUIRE(SodiumRuntime::Ready());

	const int verified = crypto_sign_verify_detached(
		signature->Data(),
		altered.data(),
		altered.size(),
		identity->publicKey.Data()
	);

	REQUIRE(verified != 0);
}

TEST_CASE("reader refuses a certificate without a signature") {
	const auto missing = RevocationCertificateReader::FromDocument(
		R"({"fingerprint":"0011223344556677","revokedAt":"2026-08-29","reason":"test"})"
	);

	REQUIRE_FALSE(missing.has_value());

	const auto malformed = RevocationCertificateReader::FromDocument(
		R"({"fingerprint":"0011223344556677","revokedAt":"2026-08-29","reason":"test","signature":"zz"})"
	);

	REQUIRE_FALSE(malformed.has_value());

	const auto discarded = RevocationCertificateReader::FromDocument("not json");

	REQUIRE_FALSE(discarded.has_value());
}

TEST_CASE("verifier accepts a self signed certificate and rejects a foreign one") {
	const TemporaryFolder folder("revocationverify");

	SigningKeyStore store;
	const auto identity = store.Create(folder.Value() / "publisher.wgrdkey", PUBLISHER, PASSPHRASE);
	REQUIRE(identity.has_value());

	SigningKeyStore attackerStore;
	const auto attacker = attackerStore.Create(folder.Value() / "attacker.wgrdkey", PUBLISHER, PASSPHRASE);
	REQUIRE(attacker.has_value());

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		identity->fingerprint.ToHex(),
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON
	);

	const auto signature = store.Sign(signable);
	REQUIRE(signature.has_value());

	const auto exported = PublisherRevocationExporter::Export(
		*identity,
		*signature,
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON,
		folder.Value() / "revoked",
		true
	);

	REQUIRE(exported.has_value());

	const auto certificate = RevocationCertificateReader::FromFile(*exported);
	REQUIRE(certificate.has_value());

	REQUIRE(RevocationVerifier::Accepts(*certificate, identity->publicKey));
	REQUIRE_FALSE(RevocationVerifier::Accepts(*certificate, attacker->publicKey));
}

TEST_CASE("revocation export refuses to replace when told not to") {
	const TemporaryFolder folder("revocationkeep");
	SigningKeyStore store;

	const auto identity = store.Create(
		folder.Value() / "publisher.wgrdkey",
		PUBLISHER,
		PASSPHRASE
	);
	REQUIRE(identity.has_value());

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		identity->fingerprint.ToHex(),
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON
	);

	const auto signature = store.Sign(signable);
	REQUIRE(signature.has_value());

	const std::filesystem::path destination = folder.Value() / "revoked";

	REQUIRE(PublisherRevocationExporter::Export(
		*identity,
		*signature,
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON,
		destination,
		true
	).has_value());

	const auto second = PublisherRevocationExporter::Export(
		*identity,
		*signature,
		REVOKED_AT,
		PublisherRevocationExporter::DEFAULT_REASON,
		destination,
		false
	);

	REQUIRE_FALSE(second.has_value());
	REQUIRE(second.error() == wgrd::manager::PublisherRevocationExportError::AlreadyPresent);
}
