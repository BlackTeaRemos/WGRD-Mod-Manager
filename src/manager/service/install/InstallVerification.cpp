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
std::expected<void, domain::InstallStartError> InstallService::Verify(const std::string_view identifier) {
	if (_busy) {
		return std::unexpected(domain::InstallStartError::Busy);
	}

	const auto announce = _receiver->Retained(std::string(identifier));
	if (!announce.has_value()) {
		return std::unexpected(domain::InstallStartError::UnknownIdentifier);
	}

	const auto decoded = DecodeStoredManifest_(announce->manifestDigest.ToHex());
	const auto ownership = CheckName_(*announce);
	if (!ownership.has_value()) {
		return std::unexpected(ownership.error());
	}
	if (!decoded.has_value()) {
		return std::unexpected(decoded.error());
	}

	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return std::unexpected(domain::InstallStartError::Busy);
	}

	JoinWorker_();
	_detailsOnly = false;

	{
		const std::scoped_lock lock(_guard);

		_announce = *announce;
		_target = *decoded;
		_progress = domain::InstallProgress{};
		_progress.identifier = _target.Identifier();
		_progress.modName = _target.ModName();
		_progress.version = _target.Version();
		_progress.remoteBytes = _target.TotalBytes();
		_progress.verification = true;
	}

	Publish_(domain::InstallPhase::Verifying, std::string(text::VERIFY_CHECKING));

	_worker = std::thread([this]() {
			RunVerify_();
		}
	);

	return {};
}

void InstallService::RunVerify_() {
	domain::ModManifest target;

	{
		const std::scoped_lock lock(_guard);
		target = _target;
	}

	const std::filesystem::path modFolder = _modsDirectory / target.ModName();

	std::error_code failure;
	if (!std::filesystem::is_directory(modFolder, failure)) {
		Conclude_(domain::InstallPhase::Failed, std::string(text::VERIFY_FOLDER_MISSING));
		return;
	}

	const InstalledContentAuditor auditor(*_hasher);

	InstalledContentAuditor::Audit audit = auditor.Examine(
		target,
		modFolder,
		[this](const std::uint64_t verifiedBytes) {
			const std::scoped_lock lock(_guard);
			_progress.fetchedBytes = verifiedBytes;
		}
	);

	if (audit.damagedChunks == 0) {
		Conclude_(domain::InstallPhase::Done, std::string(text::VERIFY_INTACT));
		return;
	}

	Publish_(domain::InstallPhase::Planning, std::string(text::VERIFY_REPAIRING));

	StartFetch_(target, domain::InstallPlan(std::move(audit.damagedFiles), {}));
}


}
