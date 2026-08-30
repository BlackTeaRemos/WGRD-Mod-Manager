#include "domain/BuildInfo.h"
#include "downloader/chunk/FastCdcChunker.h"
#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"
#include "downloader/transfer/TorrentSession.h"
#include "gui/Application.h"
#include "gui/platform/Win32FilePicker.h"
#include "gui/platform/Win32UriLauncher.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/environment/GameLocator.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/CatalogService.h"
#include "manager/service/InstallService.h"
#include "manager/service/ModRemovalService.h"
#include "manager/service/OrderService.h"
#include "manager/profile/SteamUserdataLocator.h"
#include "manager/service/ProfileService.h"
#include "manager/service/PublishService.h"
#include "manager/service/PatcherService.h"
#include "manager/service/RegistryUpdateService.h"
#include "manager/service/UpdateService.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"
#include "manager/update/ExecutableSwapper.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr std::string_view DATA_FOLDER = ".wgrdmm";
constexpr std::string_view REGISTRY_FOLDER = "registry";
constexpr std::string_view CONTROL_FOLDER = "control";
constexpr std::string_view ANNOUNCE_FOLDER = "announces";
constexpr std::string_view PROFILE_FOLDER = "profiles";
constexpr std::string_view INSTALLED_FOLDER = "installed";

std::optional<wgrd::domain::GameInstallation> LocateGame() {
	std::optional<wgrd::domain::GameInstallation> installation = wgrd::manager::GameLocator::Resolve();
	if (installation) {
		return installation;
	}

	const std::vector<wgrd::domain::GameInstallation> detected = wgrd::manager::GameLocator::Detect();
	if (!detected.empty()) {
		return detected.front();
	}

	return std::nullopt;
}
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	wgrd::manager::ExecutableSwapper::DiscardRetired();

	const std::optional<wgrd::domain::GameInstallation> installation = LocateGame();

	const wgrd::downloader::FastCdcChunker chunker(wgrd::domain::ChunkSizes::Default());
	const wgrd::manager::Blake3Hasher hasher;
	const wgrd::manager::PayloadPathPolicy pathPolicy;
	const wgrd::manager::ManifestCodec codec(pathPolicy);
	const wgrd::downloader::ChunkSetTorrentBuilder torrentBuilder;
	const wgrd::manager::ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

	std::unique_ptr<wgrd::manager::OrderService> orderService;
	std::unique_ptr<wgrd::manager::ProfileService> profileService;
	std::unique_ptr<wgrd::manager::DirectoryKeyRegistry> registry;
	std::unique_ptr<wgrd::manager::AnnounceStore> announceStore;
	std::unique_ptr<wgrd::manager::AnnounceReceiver> receiver;
	std::unique_ptr<wgrd::manager::ManifestAuthenticator> authenticator;
	std::unique_ptr<wgrd::manager::PublishService> publishService;
	std::unique_ptr<wgrd::manager::CatalogService> catalogService;
	std::unique_ptr<wgrd::manager::RegistryUpdateService> registryUpdater;
	std::unique_ptr<wgrd::manager::PatcherService> patcherService;
	std::unique_ptr<wgrd::manager::InstalledReleaseStore> installedStore;
	std::unique_ptr<wgrd::manager::InstallService> installService;
	std::unique_ptr<wgrd::manager::ModRemovalService> removalService;

	const std::filesystem::path swarmSavePath = installation
	                                            ? installation->modsDirectory
	                                            : std::filesystem::current_path();

	wgrd::downloader::TorrentSession swarm(swarmSavePath);

	if (installation) {
		const std::filesystem::path dataDirectory = installation->modsDirectory / DATA_FOLDER;

		orderService = std::make_unique<wgrd::manager::OrderService>(*installation);

		profileService = std::make_unique<wgrd::manager::ProfileService>(
			dataDirectory / PROFILE_FOLDER,
			*orderService,
			wgrd::manager::SteamUserdataLocator::Accounts()
		);

		orderService->SetChangeHandler([&profileService]() {
				if (profileService) {
					profileService->SyncActive();
				}
			}
		);

		registry = std::make_unique<wgrd::manager::DirectoryKeyRegistry>(dataDirectory / REGISTRY_FOLDER);

		announceStore = std::make_unique<wgrd::manager::AnnounceStore>(dataDirectory / ANNOUNCE_FOLDER);
		receiver = std::make_unique<wgrd::manager::AnnounceReceiver>(*registry, announceStore.get());

		const std::size_t restored = receiver->Restore();
		(void)restored;

		authenticator = std::make_unique<wgrd::manager::ManifestAuthenticator>(*registry);

		publishService = std::make_unique<wgrd::manager::PublishService>(
			installation->modsDirectory,
			dataDirectory,
			chunker,
			hasher,
			pathPolicy,
			torrentBuilder,
			*receiver,
			*receiver,
			*registry
		);

		installedStore = std::make_unique<wgrd::manager::InstalledReleaseStore>(
			dataDirectory / INSTALLED_FOLDER
		);

		catalogService = std::make_unique<wgrd::manager::CatalogService>(
			installation->modsDirectory,
			*receiver,
			publishService->Store(),
			*authenticator,
			codec,
			*registry,
			*installedStore,
			&swarm
		);

		installService = std::make_unique<wgrd::manager::InstallService>(
			installation->modsDirectory,
			dataDirectory,
			*receiver,
			publishService->Store(),
			*authenticator,
			codec,
			manifestBuilder,
			hasher,
			swarm,
			*installedStore
		);

		removalService = std::make_unique<wgrd::manager::ModRemovalService>(
			installation->modsDirectory,
			*installedStore,
			*receiver,
			orderService.get(),
			&swarm,
			installService.get()
		);

		registryUpdater = std::make_unique<wgrd::manager::RegistryUpdateService>(
			std::string(wgrd::domain::build::INDEX_REPOSITORY),
			dataDirectory / REGISTRY_FOLDER,
			*registry
		);

		patcherService = std::make_unique<wgrd::manager::PatcherService>(
			std::string(wgrd::domain::build::PATCHER_REPOSITORY),
			installation->root,
			dataDirectory
		);

		swarm.StartGossip(*receiver, *receiver, dataDirectory / CONTROL_FOLDER);
	}

	const auto currentVersion = wgrd::manager::VersionNumber::Parse(wgrd::domain::build::VERSION);
	wgrd::manager::UpdateService updateService(
		std::string(wgrd::domain::build::RELEASE_REPOSITORY),
		currentVersion.value_or(wgrd::manager::VersionNumber())
	);

	wgrd::gui::Win32FilePicker filePicker(nullptr);
	const wgrd::gui::Win32UriLauncher uriLauncher;

	const wgrd::gui::ApplicationServices services{
		orderService.get(), catalogService.get(), publishService.get(), &swarm, &swarm, installService.get(), &updateService, registryUpdater.get(), &filePicker, &uriLauncher, &swarm, profileService.get()
		, removalService.get(), patcherService.get()
	};

	wgrd::gui::Application application;

	if (!application.Start(services)) {
		return 1;
	}

	filePicker.SetOwner(application.WindowHandle());

	return application.Run();
}
