#include "manager/service/UpdateService.h"

#include "manager/update/ExecutableSwapper.h"
#include "manager/update/ReleaseLookupText.h"

#include <string>
#include <utility>

namespace wgrd::manager {
UpdateService::UpdateService(std::string repository, const VersionNumber current)
	: _client()
	, _source(_client)
	, _repository(std::move(repository))
	, _current(current)
	, _guard()
	, _status()
	, _available()
	, _worker()
	, _busy(false) {
	_status.currentVersion = _current.ToText();
	_status.phase = domain::UpdatePhase::Idle;
	_status.message = "not checked";
}

UpdateService::~UpdateService() {
	JoinWorker_();
}

void UpdateService::JoinWorker_() {
	if (_worker.joinable()) {
		_worker.join();
	}
}

void UpdateService::Publish_(const domain::UpdatePhase phase, std::string message) {
	const std::scoped_lock lock(_guard);
	_status.phase = phase;
	_status.message = std::move(message);
}

domain::UpdateStatus UpdateService::Status() const {
	const std::scoped_lock lock(_guard);
	return _status;
}

void UpdateService::RunCheck_() {
	const auto release = _source.Latest(_repository);

	if (!release.has_value()) {
		Publish_(domain::UpdatePhase::Failed, std::string(ReleaseLookupText::Describe(release.error())));
		_busy = false;
		return;
	}

	{
		const std::scoped_lock lock(_guard);
		_status.latestVersion = release->version.ToText();
		_status.totalBytes = release->assetBytes;
	}

	if (release->version <= _current) {
		_available.reset();
		Publish_(domain::UpdatePhase::UpToDate, "already current");
		_busy = false;
		return;
	}

	_available = *release;
	Publish_(domain::UpdatePhase::Available, "update available");
	_busy = false;
}

void UpdateService::RunDownload_() {
	if (!_available.has_value()) {
		Publish_(domain::UpdatePhase::Failed, "nothing to download");
		_busy = false;
		return;
	}

	const std::filesystem::path staged = ExecutableSwapper::StagedPath();
	if (staged.empty()) {
		Publish_(domain::UpdatePhase::Failed, "executable unknown");
		_busy = false;
		return;
	}

	{
		const std::scoped_lock lock(_guard);
		_status.downloadedBytes = 0;
	}

	const auto written = _client.Download(
		_available->assetUrl,
		staged,
		HttpsClient::PAYLOAD_LIMIT,
		[this](const std::uint64_t received) {
			const std::scoped_lock lock(_guard);
			_status.downloadedBytes = received;
		}
	);

	if (!written.has_value()) {
		Publish_(domain::UpdatePhase::Failed, "download failed");
		_busy = false;
		return;
	}

	if (_available->assetBytes != 0 && *written != _available->assetBytes) {
		std::error_code failure;
		std::filesystem::remove(staged, failure);
		Publish_(domain::UpdatePhase::Failed, "size mismatch");
		_busy = false;
		return;
	}

	{
		const std::scoped_lock lock(_guard);
		_status.downloadedBytes = *written;
	}

	Publish_(domain::UpdatePhase::Ready, "restart to apply");
	_busy = false;
}

void UpdateService::Check() {
	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return;
	}

	JoinWorker_();
	Publish_(domain::UpdatePhase::Checking, "contacting github");

	_worker = std::thread([this]() {
			RunCheck_();
		}
	);
}

void UpdateService::Download() {
	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return;
	}

	JoinWorker_();
	Publish_(domain::UpdatePhase::Downloading, "downloading");

	_worker = std::thread([this]() {
			RunDownload_();
		}
	);
}

bool UpdateService::ApplyAndRestart() {
	if (_busy) {
		return false;
	}

	JoinWorker_();

	const auto applied = ExecutableSwapper::Apply();
	if (!applied.has_value()) {
		Publish_(domain::UpdatePhase::Failed, "swap failed");
		return false;
	}

	return true;
}
}
