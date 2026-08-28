#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IProfileService.h"
#include "domain/types/profile/SteamAccount.h"
#include "manager/profile/ProfileStore.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wgrd::manager {
class ProfileService final : public domain::IProfileService {
public:
	static constexpr std::string_view DEFAULT_NAME = domain::DEFAULT_PROFILE_NAME;
	static constexpr std::string_view SHARED_FOLDER = "shared";

	ProfileService(
		std::filesystem::path profilesFolder,
		domain::IOrderService& order,
		std::vector<domain::SteamAccount> accounts
	);

	~ProfileService() override;

	[[nodiscard]] const std::vector<domain::ProfileSummary>& Profiles() const override;

	[[nodiscard]] const std::string& Active() const override;

	[[nodiscard]] const std::vector<domain::GameProfileFile>& Discovered() const override;

	[[nodiscard]] std::expected<void, domain::ProfileError> SetDefault(
		const domain::GameProfileFile& discovered
	) override;

	[[nodiscard]] std::optional<domain::Profile> Load(std::string_view name) const override;

	[[nodiscard]] std::expected<void, domain::ProfileError> CaptureCurrent(
		std::string_view name
	) override;

	[[nodiscard]] std::expected<void, domain::ProfileError> Clone(
		std::string_view source,
		std::string_view name
	) override;

	[[nodiscard]] std::expected<void, domain::ProfileError> Activate(std::string_view name) override;

	[[nodiscard]] std::expected<void, domain::ProfileError> Remove(std::string_view name) override;

	void Refresh() override;

	[[nodiscard]] const std::string& LastMessage() const override;

	[[nodiscard]] const std::string& CurrentAccount() const;

	void SyncActive();

private:
	[[nodiscard]] domain::LoadOrder ComposeLive_() const;

	[[nodiscard]] std::size_t MissingCount_(const domain::Profile& profile) const;

	[[nodiscard]] std::filesystem::path FolderFor_(std::string_view account) const;

	[[nodiscard]] std::filesystem::path LivePathFor_(std::string_view account) const;

	[[nodiscard]] static std::string CurrentOf_(const std::vector<domain::SteamAccount>& accounts);

	void EnsureDefaults_();

	void RefreshSummaries_();

	std::filesystem::path _profilesRoot;
	domain::IOrderService* _order;
	std::vector<domain::SteamAccount> _accounts;
	std::string _current;
	std::unique_ptr<ProfileStore> _store;
	std::vector<domain::ProfileSummary> _summaries;
	std::vector<domain::GameProfileFile> _discovered;
	std::string _active;
	std::string _message;
};
}
