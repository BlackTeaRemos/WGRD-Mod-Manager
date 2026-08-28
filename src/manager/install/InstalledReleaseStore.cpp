#include "manager/install/InstalledReleaseStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::manager {
namespace {
	bool IsAcceptableStemCharacter(const char character) {
		const bool lower = character >= 'a' && character <= 'z';
		const bool upper = character >= 'A' && character <= 'Z';
		const bool digit = character >= '0' && character <= '9';

		return lower || upper || digit || character == '_' || character == '-' || character == '.';
	}
}

InstalledReleaseStore::InstalledReleaseStore(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

InstalledReleaseStore::~InstalledReleaseStore() = default;

std::string InstalledReleaseStore::FileStemFor_(const std::string_view identifier) {
	std::string stem;
	stem.reserve(identifier.size());

	for (const char character : identifier) {
		stem.push_back(character == '/' ? '_' : character);
	}

	if (!std::ranges::all_of(stem, IsAcceptableStemCharacter)) {
		return {};
	}

	if (stem.find("..") != std::string::npos) {
		return {};
	}

	return stem;
}

std::filesystem::path InstalledReleaseStore::PathFor_(const std::string_view identifier) const {
	return _folder / (FileStemFor_(identifier) + std::string(FILE_EXTENSION));
}

std::optional<domain::InstalledRelease> InstalledReleaseStore::Find(
	const std::string_view identifier
) const {
	if (FileStemFor_(identifier).empty()) {
		return std::nullopt;
	}

	std::ifstream input(PathFor_(identifier), std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	const std::string document(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	const nlohmann::json parsed = nlohmann::json::parse(document, nullptr, false);

	if (parsed.is_discarded() || !parsed.is_object()) {
		return std::nullopt;
	}

	if (!parsed.contains("identifier") || !parsed["identifier"].is_string()) {
		return std::nullopt;
	}

	if (!parsed.contains("modName") || !parsed["modName"].is_string()) {
		return std::nullopt;
	}

	if (!parsed.contains("version") || !parsed["version"].is_number_unsigned()) {
		return std::nullopt;
	}

	if (!parsed.contains("manifestDigest") || !parsed["manifestDigest"].is_string()) {
		return std::nullopt;
	}

	return domain::InstalledRelease{
		parsed["identifier"].get<std::string>(), parsed["modName"].get<std::string>(), parsed["version"].get<std::uint64_t>(), parsed["manifestDigest"].get<std::string>()
	};
}

std::vector<domain::InstalledRelease> InstalledReleaseStore::LoadAll() const {
	std::vector<domain::InstalledRelease> releases;

	std::error_code failure;
	if (!std::filesystem::is_directory(_folder, failure)) {
		return releases;
	}

	std::filesystem::directory_iterator walker(_folder, failure);
	if (failure) {
		return releases;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || releases.size() >= RECORD_LIMIT) {
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

		const nlohmann::json parsed = nlohmann::json::parse(document, nullptr, false);

		if (parsed.is_discarded() || !parsed.is_object()) {
			continue;
		}

		if (!parsed.contains("identifier") || !parsed["identifier"].is_string()) {
			continue;
		}

		if (auto found = Find(parsed["identifier"].get<std::string>())) {
			releases.push_back(std::move(*found));
		}
	}

	return releases;
}

bool InstalledReleaseStore::Save(const domain::InstalledRelease& release) const {
	if (FileStemFor_(release.identifier).empty()) {
		return false;
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);
	if (failure) {
		return false;
	}

	nlohmann::json document;
	document["identifier"] = release.identifier;
	document["modName"] = release.modName;
	document["version"] = release.version;
	document["manifestDigest"] = release.manifestDigest;

	std::ofstream output(PathFor_(release.identifier), std::ios::binary | std::ios::trunc);
	if (!output) {
		return false;
	}

	output << document.dump(2) << "\n";

	return static_cast<bool>(output);
}

bool InstalledReleaseStore::Remove(const std::string_view identifier) const {
	if (FileStemFor_(identifier).empty()) {
		return false;
	}

	std::error_code failure;
	const bool removed = std::filesystem::remove(PathFor_(identifier), failure);

	return removed && !failure;
}
}
