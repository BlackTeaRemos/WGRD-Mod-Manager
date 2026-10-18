#include "manager/publish/PublisherKeyExporter.h"
#include "manager/publish/PublisherRevocationExporter.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"
#include "manager/trust/ManifestEnvelope.h"
#include "manager/trust/RevocationSignable.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <atomic>
#include <string>
#include <system_error>
#include <vector>

namespace {
class TemporaryFolder {
public:
	TemporaryFolder() {
		static std::atomic<unsigned long> nextIdentifier = 0;
		_root = std::filesystem::temp_directory_path() / ("wgrd-reload-" + std::to_string(nextIdentifier.fetch_add(1)));
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
		std::filesystem::create_directories(_root, failure);
	}
	~TemporaryFolder() {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}
	[[nodiscard]] const std::filesystem::path& Root() const {
		return _root;
	}
private:
	std::filesystem::path _root;
};
}

TEST_CASE("T09 revoked publisher rejected after reload") {
	using namespace wgrd::manager;
	TemporaryFolder folder;
	SigningKeyStore store;
	const auto identity = store.Create(folder.Root() / "publisher.wgrdkey", "tester", "correct horse battery");
	REQUIRE(identity.has_value());
	const auto keys = folder.Root() / "registry" / DirectoryKeyRegistry::KEYS_FOLDER;
	REQUIRE(PublisherKeyExporter::Export(*identity, "tester", "2026-08-29", keys, true).has_value());
	DirectoryKeyRegistry registry(folder.Root() / "registry");
	const std::vector<std::uint8_t> payload{'p', 'a', 'y', 'l', 'o', 'a', 'd'};
	const auto signature = store.Sign(payload);
	REQUIRE(signature.has_value());
	const auto envelope = ManifestEnvelope::Encode(identity->fingerprint, *signature, payload);
	ManifestAuthenticator authenticator(registry);
	REQUIRE(authenticator.Authenticate(envelope).has_value());
	const auto signable = RevocationSignable::Bytes(identity->fingerprint.ToHex(), "2026-08-29", PublisherRevocationExporter::DEFAULT_REASON);
	const auto revocationSignature = store.Sign(signable);
	REQUIRE(revocationSignature.has_value());
	REQUIRE(PublisherRevocationExporter::Export(*identity, *revocationSignature, "2026-08-29", PublisherRevocationExporter::DEFAULT_REASON, folder.Root() / "registry" / DirectoryKeyRegistry::REVOKED_FOLDER, true).has_value());
	registry.Reload();

	const auto rejected = authenticator.Authenticate(envelope);

	REQUIRE_FALSE(rejected.has_value());
	REQUIRE(rejected.error() == wgrd::domain::ManifestAuthenticationError::RevokedPublisher);
}
