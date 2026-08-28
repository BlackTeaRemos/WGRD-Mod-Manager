#pragma once

#include "domain/types/order/Profile.h"
#include "domain/types/profile/GameProfileFile.h"
#include "domain/types/status/ProfileSummary.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::domain {
inline constexpr std::string_view DEFAULT_PROFILE_NAME = "default";

enum class ProfileError {
	NameRejected
	, NameTaken
	, NotFound
	, NoInstallation
	, Unwritable
	, Unreadable
	, ApplyFailed
	, GameProfileMissing
	, GameProfileRejected
	, DefaultProtected
};

class IProfileService {
public:
	virtual ~IProfileService() = 0;

	[[nodiscard]] virtual const std::vector<ProfileSummary>& Profiles() const = 0;

	[[nodiscard]] virtual const std::string& Active() const = 0;

	[[nodiscard]] virtual const std::vector<GameProfileFile>& Discovered() const = 0;

	[[nodiscard]] virtual std::expected<void, ProfileError> SetDefault(
		const GameProfileFile& discovered
	) = 0;

	[[nodiscard]] virtual std::optional<Profile> Load(std::string_view name) const = 0;

	[[nodiscard]] virtual std::expected<void, ProfileError> CaptureCurrent(std::string_view name) = 0;

	[[nodiscard]] virtual std::expected<void, ProfileError> Clone(
		std::string_view source,
		std::string_view name
	) = 0;

	[[nodiscard]] virtual std::expected<void, ProfileError> Activate(std::string_view name) = 0;

	[[nodiscard]] virtual std::expected<void, ProfileError> Remove(std::string_view name) = 0;

	virtual void Refresh() = 0;

	[[nodiscard]] virtual const std::string& LastMessage() const = 0;
};

inline IProfileService::~IProfileService() = default;
}
