#pragma once

#include "domain/interfaces/services/IPatcherService.h"
#include "manager/patcher/PatcherMarker.h"
#include "manager/update/GitHubReleaseSource.h"
#include "manager/update/HttpsClient.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace wgrd::manager {
class PatcherService final : public domain::IPatcherService {
public:
	static constexpr std::string_view ASSET_NAME = "version.dll";
	static constexpr std::string_view STAGED_NAME = "version.dll.staged";

	PatcherService(
		std::string repository,
		std::filesystem::path gameRoot,
		std::filesystem::path dataDirectory
	);

	PatcherService(const PatcherService&) = delete;

	PatcherService& operator=(const PatcherService&) = delete;

	~PatcherService() override;

	[[nodiscard]] domain::PatcherStatus Status() const override;

	void Check() override;

	void Install() override;

private:
	void RunCheck_();

	void RunInstall_();

	void JoinWorker_();

	void Publish_(domain::UpdatePhase phase, std::string message);

	void RefreshPresence_();

	HttpsClient _client;
	GitHubReleaseSource _source;
	PatcherMarker _marker;
	std::string _repository;
	std::filesystem::path _gameRoot;
	std::filesystem::path _dataDirectory;

	mutable std::mutex _guard;
	domain::PatcherStatus _status;
	std::optional<ReleaseDescription> _available;

	std::thread _worker;
	std::atomic<bool> _busy;
};
}
