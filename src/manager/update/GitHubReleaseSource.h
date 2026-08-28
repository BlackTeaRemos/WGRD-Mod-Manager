#pragma once

#include "manager/update/HttpsClient.h"
#include "manager/update/VersionNumber.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace wgrd::manager {
enum class ReleaseLookupError {
	RepositoryUnset
	, RequestFailed
	, ResponseMalformed
	, TagRejected
	, AssetMissing
	, AssetUrlRejected
};

struct ReleaseDescription {
	VersionNumber version;
	std::string tag;
	std::string assetUrl;
	std::uint64_t assetBytes;
};

class GitHubReleaseSource {
public:
	static constexpr std::string_view ASSET_NAME = "wgrd-mod-manager.exe";
	static constexpr std::string_view ACCEPT_HEADER = "application/vnd.github+json";
	static constexpr std::string_view API_HOST = "https://api.github.com";

	explicit GitHubReleaseSource(const HttpsClient& client);

	[[nodiscard]] std::expected<ReleaseDescription, ReleaseLookupError> Latest(
		std::string_view repository,
		std::string_view assetName = ASSET_NAME
	) const;

	[[nodiscard]] static std::expected<ReleaseDescription, ReleaseLookupError> ParseRelease(
		std::string_view document,
		std::string_view assetName = ASSET_NAME
	);

	[[nodiscard]] static std::string BuildUrl(std::string_view repository);

private:
	const HttpsClient* _client;
};
}
