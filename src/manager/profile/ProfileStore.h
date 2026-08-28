#pragma once

#include "domain/types/order/Profile.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class ProfileStore {
public:
	static constexpr std::string_view FILE_EXTENSION = ".wgrdp";
	static constexpr std::string_view GAME_PROFILE_EXTENSION = ".wargameprofile";
	static constexpr std::string_view ACTIVE_FILE = "active";
	static constexpr std::size_t PROFILE_LIMIT = 256;

	explicit ProfileStore(std::filesystem::path folder);

	~ProfileStore();

	[[nodiscard]] bool Holds(std::string_view name) const;

	[[nodiscard]] std::optional<domain::Profile> Load(std::string_view name) const;

	[[nodiscard]] std::vector<domain::Profile> LoadAll() const;

	[[nodiscard]] bool Save(const domain::Profile& profile) const;

	[[nodiscard]] bool Remove(std::string_view name) const;

	[[nodiscard]] std::string ReadActive() const;

	[[nodiscard]] bool WriteActive(std::string_view name) const;

	[[nodiscard]] std::filesystem::path GameProfilePathFor(std::string_view name) const;

	[[nodiscard]] bool HoldsGameProfile(std::string_view name) const;

private:
	[[nodiscard]] std::filesystem::path PathFor_(std::string_view name) const;

	std::filesystem::path _folder;
};
}
