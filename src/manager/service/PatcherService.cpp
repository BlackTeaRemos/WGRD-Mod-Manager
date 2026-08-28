#include "manager/service/PatcherService.h"

#include "manager/patcher/PatcherInstaller.h"
#include "manager/patcher/PatcherRuntimeVersion.h"
#include "manager/text/ServiceText.h"
#include "manager/update/ReleaseLookupText.h"

#include <string_view>
#include <system_error>
#include <utility>

namespace wgrd::manager {
PatcherService::PatcherService(
	std::string repository,
	std::filesystem::path gameRoot,
	std::filesystem::path dataDirectory
)
	: _client()
	, _source(_client)
	, _marker(dataDirectory)
	, _repository(std::move(repository))
	, _gameRoot(std::move(gameRoot))
	, _dataDirectory(std::move(dataDirectory))
	, _guard()
	, _status()
	, _available()
	, _worker()
	, _busy(false) {
	RefreshPresence_();
}

PatcherService::~PatcherService() {
	JoinWorker_();
}

void PatcherService::RefreshPresence_() {
	const std::scoped_lock lock(_guard);

	_status.present = PatcherInstaller::Installed(_gameRoot);
	_status.installedTag = _marker.Read();
	_status.runtimeStamp = PatcherRuntimeVersion::Read(
		_gameRoot / std::string(PatcherInstaller::MODS_FOLDER)
	);
}

domain::PatcherStatus PatcherService::Status() const {
	const std::scoped_lock lock(_guard);
	return _status;
}

void PatcherService::Publish_(const domain::UpdatePhase phase, std::string message) {
	const std::scoped_lock lock(_guard);

	_status.phase = phase;
	_status.message = std::move(message);
}

void PatcherService::JoinWorker_() {
	if (_worker.joinable()) {
		_worker.join();
	}
}

void PatcherService::Check() {
	if (_busy.exchange(true)) {
		return;
	}

	JoinWorker_();

	Publish_(domain::UpdatePhase::Checking, std::string(text::PATCHER_CHECKING));

	_worker = std::thread([this]() {
			RunCheck_();
			_busy.store(false);
		}
	);
}

void PatcherService::RunCheck_() {
	if (_repository.empty()) {
		Publish_(domain::UpdatePhase::Failed, std::string(text::PATCHER_REPOSITORY_UNSET));
		return;
	}

	const auto latest = _source.Latest(_repository, ASSET_NAME);
	if (!latest.has_value()) {
		Publish_(domain::UpdatePhase::Failed, std::string(ReleaseLookupText::Describe(latest.error())));
		return;
	}

	const std::scoped_lock lock(_guard);

	_available = *latest;
	_status.latestTag = latest->tag;
	_status.present = PatcherInstaller::Installed(_gameRoot);
	_status.installedTag = _marker.Read();
	_status.runtimeStamp = PatcherRuntimeVersion::Read(
		_gameRoot / std::string(PatcherInstaller::MODS_FOLDER)
	);

	const bool current = _status.present && _status.installedTag == latest->tag;

	_status.phase = current ? domain::UpdatePhase::UpToDate : domain::UpdatePhase::Available;
	_status.message = current
	                  ? std::string(text::PATCHER_CURRENT)
	                  : std::string(text::PATCHER_AVAILABLE);
}

void PatcherService::Install() {
	if (_busy.exchange(true)) {
		return;
	}

	JoinWorker_();

	Publish_(domain::UpdatePhase::Downloading, std::string(text::PATCHER_DOWNLOADING));

	_worker = std::thread([this]() {
			RunInstall_();
			_busy.store(false);
		}
	);
}

void PatcherService::RunInstall_() {
	std::optional<ReleaseDescription> target;
	{
		const std::scoped_lock lock(_guard);
		target = _available;
	}

	if (!target.has_value()) {
		Publish_(domain::UpdatePhase::Failed, std::string(text::PATCHER_NOT_CHECKED));
		return;
	}

	std::error_code failure;
	std::filesystem::create_directories(_dataDirectory, failure);

	const std::filesystem::path staged = _dataDirectory / std::string(STAGED_NAME);

	const auto downloaded = _client.Download(
		target->assetUrl,
		staged,
		HttpsClient::PAYLOAD_LIMIT
	);

	if (!downloaded.has_value()) {
		Publish_(domain::UpdatePhase::Failed, std::string(text::PATCHER_DOWNLOAD_FAILED));
		return;
	}

	if (target->assetBytes != 0 && *downloaded != target->assetBytes) {
		std::filesystem::remove(staged, failure);
		Publish_(domain::UpdatePhase::Failed, std::string(text::PATCHER_SIZE_MISMATCH));
		return;
	}

	const auto installed = PatcherInstaller::Install(_gameRoot, staged);

	std::filesystem::remove(staged, failure);

	if (!installed.has_value()) {
		Publish_(domain::UpdatePhase::Failed, std::string(text::PATCHER_INSTALL_FAILED));
		return;
	}

	const bool marked = _marker.Write(target->tag);
	(void)marked;

	const std::scoped_lock lock(_guard);

	_status.present = true;
	_status.installedTag = target->tag;
	_status.phase = domain::UpdatePhase::UpToDate;
	_status.message = std::string(text::PATCHER_INSTALLED);
}
}
