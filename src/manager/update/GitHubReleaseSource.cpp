#include "manager/update/GitHubReleaseSource.h"

#include "manager/update/DownloadHostPolicy.h"

#include <nlohmann/json.hpp>

#include <format>

namespace wgrd::manager {
namespace {
	using Json = nlohmann::json;

	std::string_view StripHost(std::string_view repository) {
		constexpr std::string_view PREFIX = "github.com/";

		const std::size_t position = repository.find(PREFIX);
		if (position == std::string_view::npos) {
			return repository;
		}

		return repository.substr(position + PREFIX.size());
	}
}

GitHubReleaseSource::GitHubReleaseSource(const HttpsClient& client)
	: _client(&client) {}

std::string GitHubReleaseSource::BuildUrl(const std::string_view repository) {
	return std::format("{}/repos/{}/releases/latest", API_HOST, StripHost(repository));
}

std::expected<ReleaseDescription, ReleaseLookupError> GitHubReleaseSource::ParseRelease(
	std::string_view document,
	const std::string_view assetName
) {
	const Json parsed = Json::parse(document.begin(), document.end(), nullptr, false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		return std::unexpected(ReleaseLookupError::ResponseMalformed);
	}

	if (!parsed.contains("tag_name") || !parsed.at("tag_name").is_string()) {
		return std::unexpected(ReleaseLookupError::ResponseMalformed);
	}

	const std::string tag = parsed.at("tag_name").get<std::string>();
	const auto version = VersionNumber::Parse(tag);
	if (!version.has_value()) {
		return std::unexpected(ReleaseLookupError::TagRejected);
	}

	if (!parsed.contains("assets") || !parsed.at("assets").is_array()) {
		return std::unexpected(ReleaseLookupError::ResponseMalformed);
	}

	for (const Json& asset : parsed.at("assets")) {
		if (!asset.is_object()) {
			continue;
		}

		if (!asset.contains("name") || !asset.at("name").is_string()) {
			continue;
		}

		if (asset.at("name").get<std::string>() != assetName) {
			continue;
		}

		if (!asset.contains("browser_download_url") ||
		    !asset.at("browser_download_url").is_string()) {
			return std::unexpected(ReleaseLookupError::ResponseMalformed);
		}

		const std::string assetUrl = asset.at("browser_download_url").get<std::string>();
		if (!DownloadHostPolicy::Accepts(assetUrl)) {
			return std::unexpected(ReleaseLookupError::AssetUrlRejected);
		}

		std::uint64_t assetBytes = 0;
		if (asset.contains("size") && asset.at("size").is_number_unsigned()) {
			assetBytes = asset.at("size").get<std::uint64_t>();
		}

		return ReleaseDescription{*version, tag, assetUrl, assetBytes};
	}

	return std::unexpected(ReleaseLookupError::AssetMissing);
}

std::expected<ReleaseDescription, ReleaseLookupError> GitHubReleaseSource::Latest(
	const std::string_view repository,
	const std::string_view assetName
) const {
	if (repository.empty()) {
		return std::unexpected(ReleaseLookupError::RepositoryUnset);
	}

	const auto response = _client->Get(
		BuildUrl(repository),
		ACCEPT_HEADER,
		HttpsClient::METADATA_LIMIT
	);

	if (!response.has_value()) {
		return std::unexpected(ReleaseLookupError::RequestFailed);
	}

	const std::string_view document(
		reinterpret_cast<const char*>(response->body.data()),
		response->body.size()
	);

	return ParseRelease(document, assetName);
}
}
