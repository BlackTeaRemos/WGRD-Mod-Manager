#include "downloader/chunk/FastCdcChunker.h"
#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"
#include "downloader/transfer/TorrentSession.h"

#include "manager/announce/AnnounceReceiver.h"
#include "manager/announce/AnnounceStore.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/service/CatalogService.h"
#include "manager/service/PublishService.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"
#include "manager/trust/RevocationSignable.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

namespace {
constexpr std::string_view PUBLISHER = "tester";
constexpr std::string_view PASSPHRASE = "correcthorse";
constexpr std::string_view MOD_FOLDER = "testmod";
constexpr std::uint64_t PAYLOAD_BYTES = 3 * 1024 * 1024;

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-app-seed" / label;

		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
		std::filesystem::create_directories(_root, failure);
	}

	~TemporaryTree() {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}

	[[nodiscard]] const std::filesystem::path& Root() const {
		return _root;
	}

private:
	std::filesystem::path _root;
};

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 37 + 11) & 0xFF));
	}
}
}

TEST_CASE("publishing a mod starts seeding its chunk set") {
	const TemporaryTree tree("publish");

	const std::filesystem::path modsDirectory = tree.Root() / "Mods";
	const std::filesystem::path dataDirectory = modsDirectory / ".wgrdmm";

	std::error_code failure;
	std::filesystem::create_directories(modsDirectory, failure);

	WritePayload(modsDirectory / MOD_FOLDER / "pack.dat", PAYLOAD_BYTES);

	{
		std::ofstream metadata(modsDirectory / MOD_FOLDER / "mod.json", std::ios::binary | std::ios::trunc);
		metadata << R"({"name":"testmod","version":"1.0.0"})";
	}

	const wgrd::downloader::FastCdcChunker chunker(wgrd::domain::ChunkSizes::Default());
	const wgrd::manager::Blake3Hasher hasher;
	const wgrd::manager::PayloadPathPolicy pathPolicy;
	const wgrd::manager::ManifestCodec codec(pathPolicy);
	const wgrd::downloader::ChunkSetTorrentBuilder torrentBuilder;

	wgrd::manager::DirectoryKeyRegistry registry(dataDirectory / "registry");
	wgrd::manager::AnnounceStore announceStore(dataDirectory / "announces");
	wgrd::manager::AnnounceReceiver receiver(registry, &announceStore);
	const wgrd::manager::ManifestAuthenticator authenticator(registry);

	wgrd::downloader::TorrentSession swarm(
		modsDirectory,
		std::string(wgrd::downloader::TorrentSession::PUBLIC_INTERFACES),
		true
	);

	wgrd::manager::PublishService publishService(
		modsDirectory,
		dataDirectory,
		chunker,
		hasher,
		pathPolicy,
		torrentBuilder,
		receiver,
		receiver,
		registry
	);

	const wgrd::manager::InstalledReleaseStore installedStore(dataDirectory / "installed");

	wgrd::manager::CatalogService catalogService(
		modsDirectory,
		receiver,
		publishService.Store(),
		authenticator,
		codec,
		registry,
		installedStore,
		&swarm
	);

	const auto created = publishService.CreateKey(
		PUBLISHER,
		tree.Root() / "publisher.wgrdkey",
		PASSPHRASE
	);

	REQUIRE(created.has_value());
	REQUIRE(publishService.Publisher().present);

	publishService.RefreshCandidates();

	const auto published = publishService.Publish(MOD_FOLDER);

	INFO("publish message " << publishService.LastMessage());
	REQUIRE(published.has_value());

	catalogService.Refresh();

	REQUIRE(catalogService.Rows().size() == 1);

	const wgrd::domain::CatalogRow& row = catalogService.Rows()[0];

	INFO("row installed " << row.installed << " manifestHeld " << row.manifestHeld);
	REQUIRE(row.installed);

	const auto sealed = publishService.Store().Load(published->manifestDigest);
	REQUIRE(sealed.has_value());

	INFO("registry keys " << registry.Count());
	REQUIRE(registry.Count() > 0);

	const auto authenticated = authenticator.Authenticate(*sealed);

	INFO("authenticated " << authenticated.has_value()
		<< " error " << (authenticated.has_value() ? 0 : static_cast<int>(authenticated.error()))
	);
	REQUIRE(authenticated.has_value());

	const auto decoded = codec.Decode(authenticated->payload);
	REQUIRE(decoded.has_value());

	REQUIRE(row.manifestHeld);

	REQUIRE(swarm.Enabled());
	REQUIRE(swarm.Entries().size() == 1);

	REQUIRE(swarm.Entries()[0].modName == published->modName);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);

	bool seeding = false;

	while (std::chrono::steady_clock::now() < deadline) {
		swarm.Poll();

		if (swarm.Entries()[0].seeding) {
			seeding = true;
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	REQUIRE(seeding);

	REQUIRE(installedStore.Save(
		wgrd::domain::InstalledRelease{
			published->identifier, published->modName, published->version, published->manifestDigest
		}
	));

	catalogService.Refresh();

	REQUIRE(catalogService.Verified(published->modName));
	REQUIRE_FALSE(catalogService.Verified("angel_maps"));

	const std::filesystem::path revoked = dataDirectory / "registry" / "revoked";

	std::error_code revocationFailure;
	std::filesystem::create_directories(revoked, revocationFailure);

	const std::string fingerprint = publishService.Publisher().fingerprint;

	constexpr std::string_view REVOKED_AT = "2026-08-30";
	constexpr std::string_view REVOKED_REASON = "test revocation";

	wgrd::manager::SigningKeyStore revoker;
	REQUIRE(revoker.Unlock(tree.Root() / "publisher.wgrdkey", PASSPHRASE).has_value());

	const auto signature = revoker.Sign(
		wgrd::manager::RevocationSignable::Bytes(fingerprint, REVOKED_AT, REVOKED_REASON)
	);

	REQUIRE(signature.has_value());

	{
		std::ofstream certificate(revoked / (fingerprint + ".json"), std::ios::binary | std::ios::trunc);
		certificate << R"({"fingerprint":")" << fingerprint
			<< R"(","revokedAt":")" << REVOKED_AT
			<< R"(","reason":")" << REVOKED_REASON
			<< R"(","signature":")" << signature->ToHex() << R"("})";
	}

	catalogService.Refresh();

	REQUIRE(catalogService.Rows().size() == 1);

	const wgrd::domain::CatalogRow& afterRevocation = catalogService.Rows()[0];

	REQUIRE(afterRevocation.revoked);
	REQUIRE_FALSE(catalogService.Verified(published->modName));
	REQUIRE_FALSE(afterRevocation.manifestHeld);
	REQUIRE(afterRevocation.installed);
	REQUIRE(swarm.Entries().empty());
	REQUIRE(std::filesystem::is_directory(modsDirectory / MOD_FOLDER));
}
