#include "manager/service/MirrorService.h"

#include <utility>

namespace wgrd::manager {
MirrorService::MirrorService(
	std::filesystem::path dataDirectory,
	const domain::ICatalogService& catalog,
	domain::IInstallService& install
)
	: _preference(std::move(dataDirectory))
	, _catalog(&catalog)
	, _install(&install)
	, _lastAttempt()
	, _claimed(0)
	, _enabled(false) {
	_enabled = _preference.Load().value_or(false);
}

MirrorService::~MirrorService() = default;

bool MirrorService::Wanted_(const domain::CatalogRow& row) {
	if (row.revoked) {
		return false;
	}

	return !row.installed || row.Outdated();
}

std::string MirrorService::NextCandidate_() const {
	for (const domain::CatalogRow& row : _catalog->Rows()) {
		if (Wanted_(row)) {
			return row.identifier;
		}
	}

	return {};
}

domain::MirrorStatus MirrorService::Status() const {
	domain::MirrorStatus status;

	status.enabled = _enabled;
	status.claimed = _claimed;

	for (const domain::CatalogRow& row : _catalog->Rows()) {
		if (Wanted_(row)) {
			status.pending += 1;
		}
	}

	return status;
}

void MirrorService::SetEnabled(const bool enabled) {
	_enabled = enabled;
	_preference.Save(enabled);
}

void MirrorService::Poll() {
	if (!_enabled) {
		return;
	}

	if (_install->Progress().Busy()) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - _lastAttempt < ATTEMPT_INTERVAL) {
		return;
	}

	_lastAttempt = now;

	const std::string candidate = NextCandidate_();
	if (candidate.empty()) {
		return;
	}

	const auto started = _install->Start(candidate);
	if (!started.has_value()) {
		return;
	}

	_claimed += 1;
}
}
