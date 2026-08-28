#include "manager/publish/PublisherKeyExporter.h"

#include "domain/rules/PublisherNameRule.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

namespace wgrd::manager {
std::string PublisherKeyExporter::FileNameFor(const domain::PublisherIdentity& identity) {
	return identity.fingerprint.ToHex() + std::string(FILE_EXTENSION);
}

std::expected<std::filesystem::path, PublisherKeyExportError> PublisherKeyExporter::Export(
	const domain::PublisherIdentity& identity,
	const std::string_view publisher,
	const std::string_view addedAt,
	const std::filesystem::path& destinationFolder,
	const bool replaceExisting
) {
	if (!domain::PublisherNameRule::IsAcceptable(publisher)) {
		return std::unexpected(PublisherKeyExportError::PublisherRejected);
	}

	const std::filesystem::path target = destinationFolder / FileNameFor(identity);

	std::error_code failure;
	if (!replaceExisting && std::filesystem::exists(target, failure)) {
		return std::unexpected(PublisherKeyExportError::AlreadyPresent);
	}

	std::filesystem::create_directories(destinationFolder, failure);
	if (failure) {
		return std::unexpected(PublisherKeyExportError::Unwritable);
	}

	nlohmann::json document;
	document["fingerprint"] = identity.fingerprint.ToHex();
	document["publicKey"] = identity.publicKey.ToHex();
	document["publisher"] = std::string(publisher);
	document["addedAt"] = std::string(addedAt);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	if (!output) {
		return std::unexpected(PublisherKeyExportError::Unwritable);
	}

	output << document.dump(2) << "\n";
	if (!output) {
		return std::unexpected(PublisherKeyExportError::Unwritable);
	}

	return target;
}
}
