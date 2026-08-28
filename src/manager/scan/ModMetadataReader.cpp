#include "manager/scan/ModMetadataReader.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <system_error>

namespace wgrd::manager {
std::string ModMetadataReader::Field_(const void* document, const std::string_view key) {
	const auto& parsed = *static_cast<const nlohmann::json*>(document);

	const std::string name(key);
	if (!parsed.contains(name) || !parsed.at(name).is_string()) {
		return {};
	}

	std::string value = parsed.at(name).get<std::string>();
	if (value.size() > FIELD_LIMIT) {
		value.resize(FIELD_LIMIT);
	}

	return value;
}

domain::ModMetadata ModMetadataReader::Read(const std::filesystem::path& modDirectory) {
	domain::ModMetadata metadata;

	const std::filesystem::path manifest = modDirectory / std::string(FILE_NAME);

	std::error_code failure;
	if (!std::filesystem::is_regular_file(manifest, failure) || failure) {
		return metadata;
	}

	if (std::filesystem::file_size(manifest, failure) > MAXIMUM_BYTES || failure) {
		return metadata;
	}

	std::ifstream input(manifest, std::ios::binary);
	if (!input) {
		return metadata;
	}

	const nlohmann::json parsed = nlohmann::json::parse(input, nullptr, false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		return metadata;
	}

	metadata.present = true;
	metadata.name = Field_(&parsed, "name");
	metadata.version = Field_(&parsed, "version");
	metadata.author = Field_(&parsed, "author");
	metadata.description = Field_(&parsed, "description");
	metadata.builtFromRevision = Field_(&parsed, "built_from_revision");
	metadata.createdWith = Field_(&parsed, "created_with");

	if (parsed.contains("packs") && parsed.at("packs").is_array()) {
		metadata.packCount = parsed.at("packs").size();
	}

	return metadata;
}
}
