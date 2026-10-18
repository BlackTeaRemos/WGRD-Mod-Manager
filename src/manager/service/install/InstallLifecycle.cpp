#include "manager/service/InstallService.h"

#include "manager/install/ContentInstaller.h"
#include "manager/install/FetchPlanner.h"
#include "manager/install/FetchedManifestAdopter.h"
#include "manager/install/InstallErrorText.h"
#include "manager/install/InstalledContentAuditor.h"
#include "manager/install/StagedChunkSource.h"
#include "manager/text/ServiceText.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <system_error>
#include <utility>

namespace wgrd::manager {
void InstallService::Poll() {
	const domain::InstallPhase phase = [this]() {
		const std::scoped_lock lock(_guard);
		return _progress.phase;
	}();

	if (phase != domain::InstallPhase::Fetching) {
		return;
	}

	const domain::FetchStatus status = _fetcher->Fetch();
	if (_detailsOnly && std::chrono::steady_clock::now() >= _detailsDeadline) {
		_fetcher->Cancel();
		Conclude_(domain::InstallPhase::Failed, "Preview request timed out");
		return;
	}

	{
		const std::scoped_lock lock(_guard);
		_progress.fetchedBytes = status.fetchedBytes;
		_progress.inFlightBytes = status.inFlightBytes;
		_progress.peers = status.peers;
		_progress.pendingWrites = status.pendingWrites;
		_progress.settling = status.settling;

		if (status.settling) {
			_progress.message = std::string(text::INSTALL_SETTLING_WRITES);
		}
	}

	if (status.phase == domain::FetchPhase::Failed) {
		Conclude_(domain::InstallPhase::Failed, std::string(text::INSTALL_FETCH_FAILED));
		return;
	}

	if (status.phase != domain::FetchPhase::Complete) {
		return;
	}

	const bool awaitingManifest = [this]() {
		const std::scoped_lock lock(_guard);
		return _awaitingManifest;
	}();

	if (awaitingManifest) {
		if (!AdoptFetchedManifest_()) {
			_fetcher->Cancel();
			Conclude_(domain::InstallPhase::Failed, std::string(text::INSTALL_MANIFEST_REJECTED));
			return;
		}

		_fetcher->Cancel();

		if (_detailsOnly) {
			Conclude_(domain::InstallPhase::Done, "Details ready");
			return;
		}

		Publish_(domain::InstallPhase::Planning, std::string(text::INSTALL_COMPARING));

		JoinWorker_();

		_worker = std::thread([this]() {
				RunPlan_();
			}
		);

		return;
	}

	_fetcher->Cancel();

	Publish_(domain::InstallPhase::Installing, std::string(text::INSTALL_MATERIALISING));

	JoinWorker_();

	_worker = std::thread([this]() {
			RunInstall_();
		}
	);
}

void InstallService::Cancel() {
	_abandoned = true;
	_fetcher->Cancel();

	JoinWorker_();

	_abandoned = false;

	const bool wasBusy = _busy.exchange(false);

	Publish_(domain::InstallPhase::Idle, std::string(text::INSTALL_CANCELLED));

	if (wasBusy) {
		++_settled;
	}
}
}
