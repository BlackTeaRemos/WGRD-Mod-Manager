#pragma once

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/content/IManifestBuilder.h"
#include "domain/interfaces/content/IManifestCodec.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/trust/IManifestAuthenticator.h"
#include "domain/types/content/ModManifest.h"
#include "domain/types/distribution/InstallPlan.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/install/ContentInstaller.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/install/ManifestDiffer.h"
#include "manager/manifest/ManifestStore.h"

#include <atomic>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace wgrd::manager {
class InstallService final : public domain::IInstallService {
public:
	static constexpr std::string_view STAGING_FOLDER = "incoming";

	InstallService(
		std::filesystem::path modsDirectory,
		std::filesystem::path dataDirectory,
		const AnnounceReceiver& receiver,
		const ManifestStore& store,
		const domain::IManifestAuthenticator& authenticator,
		const domain::IManifestCodec& codec,
		const domain::IManifestBuilder& manifestBuilder,
		const domain::IContentHasher& hasher,
		domain::IChunkFetcher& fetcher,
		const InstalledReleaseStore& installed
	);

	InstallService(const InstallService&) = delete;

	InstallService& operator=(const InstallService&) = delete;

	~InstallService() override;

	[[nodiscard]] std::expected<void, domain::InstallStartError> Start(
		std::string_view identifier
	) override;

	[[nodiscard]] std::expected<void, domain::InstallStartError> Verify(
		std::string_view identifier
	) override;

	[[nodiscard]] domain::InstallProgress Progress() const override;

	[[nodiscard]] std::uint64_t CompletedInstalls() const override;

	void Poll() override;

	void Cancel() override;

private:
	void JoinWorker_();

	void RunPlan_();

	[[nodiscard]] bool AdoptFetchedManifest_();

	void BeginManifestFetch_();

	void RunInstall_();

	void RunVerify_();

	void StartFetch_(const domain::ModManifest& target, const domain::InstallPlan& plan);

	void Publish_(domain::InstallPhase phase, std::string message);

	[[nodiscard]] std::filesystem::path StagingRoot_() const;

	std::filesystem::path _modsDirectory;
	std::filesystem::path _dataDirectory;
	const AnnounceReceiver* _receiver;
	const ManifestStore* _store;
	const domain::IManifestAuthenticator* _authenticator;
	const domain::IManifestCodec* _codec;
	const domain::IManifestBuilder* _manifestBuilder;
	domain::IChunkFetcher* _fetcher;
	const InstalledReleaseStore* _installed;

	const domain::IContentHasher* _hasher;
	ManifestDiffer _differ;
	ContentInstaller _installer;

	mutable std::mutex _guard;
	domain::InstallProgress _progress;
	domain::SignedAnnounce _announce;
	domain::ModManifest _target;
	domain::InstallPlan _plan;
	bool _awaitingManifest;

	std::thread _worker;
	std::atomic<bool> _busy;
	std::atomic<std::uint64_t> _completed;
};
}
