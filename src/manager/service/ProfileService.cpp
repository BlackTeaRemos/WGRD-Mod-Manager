#include "manager/service/ProfileService.h"

#include "manager/profile/GameProfileScanner.h"
#include "manager/profile/GameProfileVault.h"
#include "manager/profile/ProfileNameRule.h"
#include "manager/text/ServiceText.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace wgrd::manager {
ProfileService::ProfileService(
	std::filesystem::path profilesFolder,
	domain::IOrderService& order,
	std::vector<domain::SteamAccount> accounts
)
	: _profilesRoot(std::move(profilesFolder))
	, _order(&order)
	, _accounts(std::move(accounts))
	, _current(CurrentOf_(_accounts))
	, _store()
	, _summaries()
	, _discovered()
	, _active()
	, _message() {
	_store = std::make_unique<ProfileStore>(FolderFor_(_current));

	EnsureDefaults_();
	Refresh();
	SyncActive();
}

ProfileService::~ProfileService() = default;

std::string ProfileService::CurrentOf_(const std::vector<domain::SteamAccount>& accounts) {
	for (const domain::SteamAccount& account : accounts) {
		if (account.current) {
			return account.identifier;
		}
	}

	return {};
}

std::filesystem::path ProfileService::FolderFor_(const std::string_view account) const {
	if (account.empty() || !ProfileNameRule::Accepts(account)) {
		return _profilesRoot / std::string(SHARED_FOLDER);
	}

	return _profilesRoot / std::string(account);
}

std::filesystem::path ProfileService::LivePathFor_(const std::string_view account) const {
	for (const domain::SteamAccount& candidate : _accounts) {
		if (candidate.identifier != account) {
			continue;
		}

		return candidate.remoteFolder
		       / (std::string(GameProfileScanner::LIVE_NAME) + std::string(GameProfileScanner::FILE_EXTENSION));
	}

	return {};
}

domain::LoadOrder ProfileService::ComposeLive_() const {
	std::vector<domain::OrderEntry> entries;

	for (const domain::OrderEntryView& view : _order->Current().entries) {
		entries.push_back(domain::OrderEntry{view.folder, view.enabled});
	}

	return domain::LoadOrder(std::move(entries));
}

std::size_t ProfileService::MissingCount_(const domain::Profile& profile) const {
	const std::vector<domain::InstalledMod>& installed = _order->Current().installed;

	std::size_t missing = 0;

	for (const domain::OrderEntry& entry : profile.Order().Entries()) {
		if (!entry.enabled) {
			continue;
		}

		const bool present = std::ranges::any_of(installed, [&](const domain::InstalledMod& mod) {
				return mod.folder == entry.folder;
			}
		);

		if (!present) {
			++missing;
		}
	}

	return missing;
}

void ProfileService::EnsureDefaults_() {
	if (!_order->Current().located) {
		return;
	}

	std::vector<std::string> identifiers;

	for (const domain::SteamAccount& account : _accounts) {
		identifiers.push_back(account.identifier);
	}

	if (identifiers.empty()) {
		identifiers.emplace_back();
	}

	for (const std::string& identifier : identifiers) {
		const ProfileStore store(FolderFor_(identifier));

		if (!store.LoadAll().empty()) {
			continue;
		}

		const std::filesystem::path live = LivePathFor_(identifier);

		if (!live.empty()) {
			const auto copied = GameProfileVault::Capture(
				live,
				store.GameProfilePathFor(DEFAULT_NAME)
			);
			(void)copied;
		}

		const domain::Profile fallback(std::string(DEFAULT_NAME), ComposeLive_(), identifier);

		if (!store.Save(fallback)) {
			continue;
		}

		const bool marked = store.WriteActive(DEFAULT_NAME);
		(void)marked;
	}
}

void ProfileService::RefreshSummaries_() {
	_active = _store->ReadActive();
	_summaries.clear();

	for (const domain::Profile& profile : _store->LoadAll()) {
		_summaries.push_back(domain::ProfileSummary{
				profile.Name(), profile.Order().Entries().size(), profile.Order().EnabledCount(), MissingCount_(profile), profile.Name() == _active, profile.Account(), _store->HoldsGameProfile(profile.Name())
			}
		);
	}
}

void ProfileService::Refresh() {
	_discovered = GameProfileScanner::ScanAll(GameProfileScanner::Preferred(_accounts));

	RefreshSummaries_();
}

void ProfileService::SyncActive() {
	if (_active.empty()) {
		return;
	}

	const auto existing = _store->Load(_active);
	if (!existing.has_value()) {
		return;
	}

	const domain::Profile mirrored(existing->Name(), ComposeLive_(), existing->Account());

	if (!_store->Save(mirrored)) {
		_message = text::PROFILE_WRITE_FAILED;
		return;
	}

	RefreshSummaries_();
}

const std::vector<domain::ProfileSummary>& ProfileService::Profiles() const {
	return _summaries;
}

const std::string& ProfileService::Active() const {
	return _active;
}

const std::string& ProfileService::CurrentAccount() const {
	return _current;
}

const std::vector<domain::GameProfileFile>& ProfileService::Discovered() const {
	return _discovered;
}

std::optional<domain::Profile> ProfileService::Load(const std::string_view name) const {
	return _store->Load(name);
}

std::expected<void, domain::ProfileError> ProfileService::SetDefault(
	const domain::GameProfileFile& discovered
) {
	const auto captured = GameProfileVault::Capture(
		discovered.path,
		_store->GameProfilePathFor(DEFAULT_NAME)
	);

	if (!captured.has_value()) {
		_message = text::PROFILE_GAME_REJECTED;
		return std::unexpected(domain::ProfileError::GameProfileRejected);
	}

	const auto existing = _store->Load(DEFAULT_NAME);

	const domain::Profile refreshed(
		std::string(DEFAULT_NAME),
		existing.has_value() ? existing->Order() : ComposeLive_(),
		discovered.account
	);

	if (!_store->Save(refreshed)) {
		_message = text::PROFILE_WRITE_FAILED;
		return std::unexpected(domain::ProfileError::Unwritable);
	}

	Refresh();
	_message = text::DEFAULT_SET;

	return {};
}

std::expected<void, domain::ProfileError> ProfileService::CaptureCurrent(const std::string_view name) {
	if (!ProfileNameRule::Accepts(name)) {
		_message = text::PROFILE_NAME_REJECTED;
		return std::unexpected(domain::ProfileError::NameRejected);
	}

	if (!_order->Current().located) {
		_message = text::NO_INSTALLATION;
		return std::unexpected(domain::ProfileError::NoInstallation);
	}

	if (_store->Holds(name)) {
		_message = text::PROFILE_NAME_TAKEN;
		return std::unexpected(domain::ProfileError::NameTaken);
	}

	const std::filesystem::path live = LivePathFor_(_current);

	std::error_code probe;

	if (!live.empty() && std::filesystem::is_regular_file(live, probe) && !probe) {
		const auto copied = GameProfileVault::Capture(live, _store->GameProfilePathFor(name));

		if (!copied.has_value()) {
			_message = text::PROFILE_GAME_REJECTED;
			return std::unexpected(domain::ProfileError::GameProfileRejected);
		}
	}

	const domain::Profile captured(std::string(name), ComposeLive_(), _current);

	if (!_store->Save(captured)) {
		_message = text::PROFILE_WRITE_FAILED;
		return std::unexpected(domain::ProfileError::Unwritable);
	}

	const bool marked = _store->WriteActive(name);
	(void)marked;

	Refresh();
	_message = std::string(text::CAPTURED_PREFIX) + std::string(name);

	return {};
}

std::expected<void, domain::ProfileError> ProfileService::Clone(
	const std::string_view source,
	const std::string_view name
) {
	if (!ProfileNameRule::Accepts(name)) {
		_message = text::PROFILE_NAME_REJECTED;
		return std::unexpected(domain::ProfileError::NameRejected);
	}

	if (_store->Holds(name)) {
		_message = text::PROFILE_NAME_TAKEN;
		return std::unexpected(domain::ProfileError::NameTaken);
	}

	const auto original = _store->Load(source);
	if (!original.has_value()) {
		_message = text::PROFILE_NOT_FOUND;
		return std::unexpected(domain::ProfileError::NotFound);
	}

	if (_store->HoldsGameProfile(source)) {
		const auto copied = GameProfileVault::Capture(
			_store->GameProfilePathFor(source),
			_store->GameProfilePathFor(name)
		);

		if (!copied.has_value()) {
			_message = text::PROFILE_GAME_REJECTED;
			return std::unexpected(domain::ProfileError::GameProfileRejected);
		}
	}

	const domain::Profile copy(std::string(name), original->Order(), original->Account());

	if (!_store->Save(copy)) {
		_message = text::PROFILE_WRITE_FAILED;
		return std::unexpected(domain::ProfileError::Unwritable);
	}

	Refresh();
	_message = std::string(text::CLONED_PREFIX) + std::string(name);

	return {};
}

std::expected<void, domain::ProfileError> ProfileService::Activate(const std::string_view name) {
	const auto profile = _store->Load(name);
	if (!profile.has_value()) {
		_message = text::PROFILE_NOT_FOUND;
		return std::unexpected(domain::ProfileError::NotFound);
	}

	if (!_order->Current().located) {
		_message = text::NO_INSTALLATION;
		return std::unexpected(domain::ProfileError::NoInstallation);
	}

	if (_store->HoldsGameProfile(name) && !profile->Account().empty()) {
		const std::filesystem::path live = LivePathFor_(profile->Account());

		if (live.empty()) {
			_message = text::PROFILE_GAME_MISSING;
			return std::unexpected(domain::ProfileError::GameProfileMissing);
		}

		const auto restored = GameProfileVault::Restore(_store->GameProfilePathFor(name), live);

		if (!restored.has_value()) {
			_message = text::PROFILE_GAME_REJECTED;
			return std::unexpected(domain::ProfileError::GameProfileRejected);
		}
	}

	if (!_order->Apply(profile->Order())) {
		_message = text::PROFILE_ORDER_FAILED;
		return std::unexpected(domain::ProfileError::ApplyFailed);
	}

	if (!_store->WriteActive(name)) {
		_message = text::PROFILE_ACTIVE_FAILED;
		return std::unexpected(domain::ProfileError::Unwritable);
	}

	Refresh();
	_message = std::string(text::ACTIVATED_PREFIX) + std::string(name);

	return {};
}

std::expected<void, domain::ProfileError> ProfileService::Remove(const std::string_view name) {
	if (name == DEFAULT_NAME) {
		_message = text::PROFILE_DEFAULT_KEPT;
		return std::unexpected(domain::ProfileError::DefaultProtected);
	}

	if (!_store->Holds(name)) {
		_message = text::PROFILE_NOT_FOUND;
		return std::unexpected(domain::ProfileError::NotFound);
	}

	if (!_store->Remove(name)) {
		_message = text::PROFILE_DELETE_FAILED;
		return std::unexpected(domain::ProfileError::Unwritable);
	}

	if (_active == name) {
		const bool cleared = _store->WriteActive(DEFAULT_NAME);
		(void)cleared;
	}

	Refresh();
	_message = std::string(text::DELETED_PREFIX) + std::string(name);

	return {};
}

const std::string& ProfileService::LastMessage() const {
	return _message;
}
}
