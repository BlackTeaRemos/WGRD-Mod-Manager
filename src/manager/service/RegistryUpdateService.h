#pragma once

#include "domain/interfaces/trust/IRegistryUpdater.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/update/GitHubContentsSource.h"
#include "manager/update/HttpsClient.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace wgrd::manager {
class RegistryUpdateService final : public domain::IRegistryUpdater {
public:
	static constexpr std::string_view KEYS_FOLDER = "keys";
	static constexpr std::string_view REVOKED_FOLDER = "revoked";
	static constexpr std::chrono::minutes REFRESH_INTERVAL{30};
	static constexpr std::chrono::seconds RETRY_INTERVAL{60};

	RegistryUpdateService(
		std::string repository,
		std::filesystem::path registryFolder,
		DirectoryKeyRegistry& registry
	);

	RegistryUpdateService(const RegistryUpdateService&) = delete;

	RegistryUpdateService& operator=(const RegistryUpdateService&) = delete;

	~RegistryUpdateService() override;

	[[nodiscard]] domain::RegistryStatus Status() const override;

	void Poll() override;

	void Tick() override;

private:
	[[nodiscard]] bool Due_() const;

	void RunPoll_();

	void JoinWorker_();

	[[nodiscard]] std::size_t FetchFolder_(std::string_view folder) const;

	HttpsClient _client;
	GitHubContentsSource _source;
	std::string _repository;
	std::filesystem::path _registryFolder;
	DirectoryKeyRegistry* _registry;

	mutable std::mutex _guard;
	domain::RegistryStatus _status;
	std::chrono::steady_clock::time_point _lastPoll;
	bool _polled;

	std::thread _worker;
	std::atomic<bool> _busy;
};
}
