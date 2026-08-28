#include "manager/update/GitHubContentsSource.h"

#include "manager/update/DownloadHostPolicy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>

namespace wgrd::manager {
namespace {
	using Json = nlohmann::json;

	constexpr std::size_t FINGERPRINT_LENGTH = 16;
	constexpr std::string_view JSON_SUFFIX = ".json";

	std::string_view StripHost(std::string_view repository) {
		constexpr std::string_view PREFIX = "github.com/";

		const std::size_t position = repository.find(PREFIX);
		if (position == std::string_view::npos) {
			return repository;
		}

		return repository.substr(position + PREFIX.size());
	}
}

GitHubContentsSource::GitHubContentsSource(const HttpsClient& client)
	: _client(&client) {}

std::string GitHubContentsSource::BuildUrl(const std::string_view repository, std::string_view folder) {
	return std::format("{}/repos/{}/contents/{}", API_HOST, StripHost(repository), folder);
}

bool GitHubContentsSource::IsRegistryFileName(const std::string_view name) {
	if (name.size() != FINGERPRINT_LENGTH + JSON_SUFFIX.size()) {
		return false;
	}

	if (!name.ends_with(JSON_SUFFIX)) {
		return false;
	}

	const std::string_view stem = name.substr(0, FINGERPRINT_LENGTH);

	return std::ranges::all_of(stem, [](const char character) {
		                   const bool digit = character >= '0' && character <= '9';
		                   const bool lower = character >= 'a' && character <= 'f';
		                   return digit || lower;
	                   }
	);
}

std::expected<std::vector<ContentsEntry>, ContentsLookupError> GitHubContentsSource::ParseListing(
	std::string_view document
) {
	const Json parsed = Json::parse(document.begin(), document.end(), nullptr, false);
	if (parsed.is_discarded() || !parsed.is_array()) {
		return std::unexpected(ContentsLookupError::ResponseMalformed);
	}

	if (parsed.size() > MAX_ENTRIES) {
		return std::unexpected(ContentsLookupError::TooManyEntries);
	}

	std::vector<ContentsEntry> entries;

	for (const Json& element : parsed) {
		if (!element.is_object()) {
			continue;
		}

		if (!element.contains("type") || !element.at("type").is_string()) {
			continue;
		}

		if (element.at("type").get<std::string>() != "file") {
			continue;
		}

		if (!element.contains("name") || !element.at("name").is_string()) {
			continue;
		}

		const std::string name = element.at("name").get<std::string>();
		if (!IsRegistryFileName(name)) {
			continue;
		}

		if (!element.contains("download_url") || !element.at("download_url").is_string()) {
			continue;
		}

		const std::string downloadUrl = element.at("download_url").get<std::string>();
		if (!DownloadHostPolicy::Accepts(downloadUrl)) {
			continue;
		}

		std::uint64_t bytes = 0;
		if (element.contains("size") && element.at("size").is_number_unsigned()) {
			bytes = element.at("size").get<std::uint64_t>();
		}

		if (bytes > MAX_ENTRY_BYTES) {
			continue;
		}

		entries.push_back(ContentsEntry{name, downloadUrl, bytes});
	}

	return entries;
}

std::expected<std::vector<ContentsEntry>, ContentsLookupError> GitHubContentsSource::List(
	const std::string_view repository,
	const std::string_view folder
) const {
	if (repository.empty()) {
		return std::unexpected(ContentsLookupError::RepositoryUnset);
	}

	const auto response = _client->Get(
		BuildUrl(repository, folder),
		ACCEPT_HEADER,
		HttpsClient::METADATA_LIMIT
	);

	if (!response.has_value()) {
		return std::unexpected(ContentsLookupError::RequestFailed);
	}

	const std::string_view document(
		reinterpret_cast<const char*>(response->body.data()),
		response->body.size()
	);

	return ParseListing(document);
}
}
