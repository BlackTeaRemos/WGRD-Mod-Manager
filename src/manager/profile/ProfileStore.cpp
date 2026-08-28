#include "manager/profile/ProfileStore.h"

#include "manager/io/BoundedLineReader.h"
#include "manager/profile/ProfileCodec.h"
#include "manager/profile/ProfileNameRule.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::manager {
ProfileStore::ProfileStore(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

ProfileStore::~ProfileStore() = default;

std::filesystem::path ProfileStore::PathFor_(const std::string_view name) const {
	return _folder / (std::string(name) + std::string(FILE_EXTENSION));
}

bool ProfileStore::Holds(const std::string_view name) const {
	if (!ProfileNameRule::Accepts(name)) {
		return false;
	}

	std::error_code failure;
	return std::filesystem::is_regular_file(PathFor_(name), failure);
}

std::optional<domain::Profile> ProfileStore::Load(const std::string_view name) const {
	if (!ProfileNameRule::Accepts(name)) {
		return std::nullopt;
	}

	std::ifstream input(PathFor_(name), std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	const std::string document(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	return ProfileCodec::Decode(document);
}

std::vector<domain::Profile> ProfileStore::LoadAll() const {
	std::vector<domain::Profile> profiles;

	std::error_code failure;
	if (!std::filesystem::is_directory(_folder, failure)) {
		return profiles;
	}

	std::filesystem::directory_iterator walker(_folder, failure);
	if (failure) {
		return profiles;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || profiles.size() >= PROFILE_LIMIT) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != FILE_EXTENSION) {
			continue;
		}

		std::ifstream input(walker->path(), std::ios::binary);
		if (!input) {
			continue;
		}

		const std::string document(
			(std::istreambuf_iterator<char>(input)),
			std::istreambuf_iterator<char>()
		);

		if (auto decoded = ProfileCodec::Decode(document)) {
			profiles.push_back(std::move(*decoded));
		}
	}

	std::ranges::sort(profiles, [](const domain::Profile& left, const domain::Profile& right) {
			return left.Name() < right.Name();
		}
	);

	return profiles;
}

bool ProfileStore::Save(const domain::Profile& profile) const {
	if (!ProfileNameRule::Accepts(profile.Name())) {
		return false;
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);
	if (failure) {
		return false;
	}

	std::ofstream output(PathFor_(profile.Name()), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output << ProfileCodec::Encode(profile) << "\n";

	return static_cast<bool>(output);
}

std::filesystem::path ProfileStore::GameProfilePathFor(const std::string_view name) const {
	return _folder / (std::string(name) + std::string(GAME_PROFILE_EXTENSION));
}

bool ProfileStore::HoldsGameProfile(const std::string_view name) const {
	if (!ProfileNameRule::Accepts(name)) {
		return false;
	}

	std::error_code failure;
	return std::filesystem::is_regular_file(GameProfilePathFor(name), failure);
}

bool ProfileStore::Remove(const std::string_view name) const {
	if (!ProfileNameRule::Accepts(name)) {
		return false;
	}

	std::error_code failure;
	std::filesystem::remove(GameProfilePathFor(name), failure);

	failure.clear();

	return std::filesystem::remove(PathFor_(name), failure) && !failure;
}

std::string ProfileStore::ReadActive() const {
	const std::optional<std::string> name = BoundedLineReader::Read(
		_folder / std::string(ACTIVE_FILE),
		ProfileNameRule::LENGTH_LIMIT
	);

	if (!name.has_value() || !ProfileNameRule::Accepts(*name)) {
		return {};
	}

	return *name;
}

bool ProfileStore::WriteActive(const std::string_view name) const {
	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);
	if (failure) {
		return false;
	}

	std::ofstream output(_folder / std::string(ACTIVE_FILE), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output << name;

	return static_cast<bool>(output);
}
}
