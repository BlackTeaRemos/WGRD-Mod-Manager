#include "manager/profile/ProfileCodec.h"

#include "manager/profile/ProfileNameRule.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace wgrd::manager {
std::string ProfileCodec::Encode(const domain::Profile& profile) {
	nlohmann::json document;
	document["name"] = profile.Name();
	document["account"] = profile.Account();

	nlohmann::json entries = nlohmann::json::array();

	for (const domain::OrderEntry& entry : profile.Order().Entries()) {
		nlohmann::json encoded;
		encoded["folder"] = entry.folder.Value();
		encoded["enabled"] = entry.enabled;
		entries.push_back(std::move(encoded));
	}

	document["entries"] = std::move(entries);

	return document.dump(2);
}

std::optional<domain::Profile> ProfileCodec::Decode(std::string_view document) {
	const nlohmann::json parsed = nlohmann::json::parse(document, nullptr, false);

	if (parsed.is_discarded() || !parsed.is_object()) {
		return std::nullopt;
	}

	if (!parsed.contains("name") || !parsed["name"].is_string()) {
		return std::nullopt;
	}

	const std::string name = parsed["name"].get<std::string>();
	if (!ProfileNameRule::Accepts(name)) {
		return std::nullopt;
	}

	if (!parsed.contains("entries") || !parsed["entries"].is_array()) {
		return std::nullopt;
	}

	const nlohmann::json& encodedEntries = parsed["entries"];
	if (encodedEntries.size() > ENTRY_LIMIT) {
		return std::nullopt;
	}

	std::vector<domain::OrderEntry> entries;
	entries.reserve(encodedEntries.size());

	for (const nlohmann::json& encoded : encodedEntries) {
		if (!encoded.is_object()) {
			return std::nullopt;
		}

		if (!encoded.contains("folder") || !encoded["folder"].is_string()) {
			return std::nullopt;
		}

		if (!encoded.contains("enabled") || !encoded["enabled"].is_boolean()) {
			return std::nullopt;
		}

		const auto folder = domain::InstallFolder::Parse(encoded["folder"].get<std::string>());
		if (!folder.has_value()) {
			return std::nullopt;
		}

		entries.push_back(domain::OrderEntry{*folder, encoded["enabled"].get<bool>()});
	}

	std::string account;
	if (parsed.contains("account") && parsed["account"].is_string()) {
		account = parsed["account"].get<std::string>();
	}

	return domain::Profile(name, domain::LoadOrder(std::move(entries)), std::move(account));
}
}
