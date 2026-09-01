#include "manager/service/RegistryUpdateService.h"

#include <format>
#include <system_error>
#include <utility>

namespace wgrd::manager {
RegistryUpdateService::RegistryUpdateService(
	std::string repository,
	std::filesystem::path registryFolder,
	DirectoryKeyRegistry& registry
)
	: _client()
	, _source(_client)
	, _repository(std::move(repository))
	, _registryFolder(std::move(registryFolder))
	, _registry(&registry)
	, _guard()
	, _status()
	, _lastPoll()
	, _polled(false)
	, _worker()
	, _busy(false) {
	_status.phase = domain::RegistryPhase::Never;
	_status.message = "never synced";
	_status.keyCount = _registry->Count();
}

RegistryUpdateService::~RegistryUpdateService() {
	JoinWorker_();
}

void RegistryUpdateService::JoinWorker_() {
	if (_worker.joinable()) {
		_worker.join();
	}
}

domain::RegistryStatus RegistryUpdateService::Status() const {
	const std::scoped_lock lock(_guard);

	domain::RegistryStatus snapshot = _status;

	if (_polled) {
		const auto elapsed = std::chrono::steady_clock::now() - _lastPoll;
		snapshot.secondsSincePoll =
				static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
	}

	return snapshot;
}

std::size_t RegistryUpdateService::FetchFolder_(const std::string_view folder) const {
	const auto listing = _source.List(_repository, folder);
	if (!listing.has_value()) {
		return 0;
	}

	const std::filesystem::path target = _registryFolder / folder;

	std::error_code failure;
	std::filesystem::create_directories(target, failure);
	if (failure) {
		return 0;
	}

	std::size_t written = 0;

	for (const ContentsEntry& entry : *listing) {
		if (!GitHubContentsSource::IsRegistryFileName(entry.name)) {
			continue;
		}

		const auto fetched = _client.Download(
			entry.downloadUrl,
			target / entry.name,
			GitHubContentsSource::MAX_ENTRY_BYTES
		);

		if (fetched.has_value()) {
			++written;
		}
	}

	return written;
}

void RegistryUpdateService::RunPoll_() {
	const std::size_t keys = FetchFolder_(KEYS_FOLDER);
	const std::size_t revoked = FetchFolder_(REVOKED_FOLDER);

	const std::size_t loaded = _registry->Reload();

	const std::scoped_lock lock(_guard);

	if (keys == 0 && revoked == 0) {
		_status.phase = domain::RegistryPhase::Failed;
		_status.message = "sync failed";
		_status.keyCount = loaded;
		_lastPoll = std::chrono::steady_clock::now();
		_busy = false;
		return;
	}

	_status.phase = domain::RegistryPhase::Fresh;
	_status.message = std::format("{} keys {} revoked", keys, revoked);
	_status.keyCount = loaded;
	_status.revokedCount = revoked;

	_lastPoll = std::chrono::steady_clock::now();
	_polled = true;

	_busy = false;
}

bool RegistryUpdateService::Due_() const {
	const std::scoped_lock lock(_guard);

	if (!_polled) {
		return _status.phase != domain::RegistryPhase::Failed
		       || std::chrono::steady_clock::now() - _lastPoll >= RETRY_INTERVAL;
	}

	return std::chrono::steady_clock::now() - _lastPoll >= REFRESH_INTERVAL;
}

void RegistryUpdateService::Tick() {
	if (_busy.load()) {
		return;
	}

	if (!Due_()) {
		return;
	}

	Poll();
}

void RegistryUpdateService::Poll() {
	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return;
	}

	JoinWorker_();

	{
		const std::scoped_lock lock(_guard);
		_status.phase = domain::RegistryPhase::Polling;
		_status.message = "contacting index";
	}

	_worker = std::thread([this]() {
			RunPoll_();
		}
	);
}
}
