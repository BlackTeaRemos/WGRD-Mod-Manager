#include "manager/publish/PublisherRevocationExporter.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

namespace wgrd::manager {
std::string PublisherRevocationExporter::FileNameFor(const domain::PublisherIdentity& identity) {
	return identity.fingerprint.ToHex() + std::string(FILE_EXTENSION);
}

std::expected<std::filesystem::path, PublisherRevocationExportError> PublisherRevocationExporter::Export(
	const domain::PublisherIdentity& identity,
	const domain::Signature& signature,
	const std::string_view revokedAt,
	const std::string_view reason,
	const std::filesystem::path& destinationFolder,
	const bool replaceExisting
) {
	const std::filesystem::path target = destinationFolder / FileNameFor(identity);

	std::error_code failure;
	if (!replaceExisting && std::filesystem::exists(target, failure)) {
		return std::unexpected(PublisherRevocationExportError::AlreadyPresent);
	}

	std::filesystem::create_directories(destinationFolder, failure);
	if (failure) {
		return std::unexpected(PublisherRevocationExportError::Unwritable);
	}

	nlohmann::json document;
	document["fingerprint"] = identity.fingerprint.ToHex();
	document["revokedAt"] = std::string(revokedAt);
	document["reason"] = std::string(reason);
	document["signature"] = signature.ToHex();

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	if (!output) {
		return std::unexpected(PublisherRevocationExportError::Unwritable);
	}

	output << document.dump(2) << "\n";
	if (!output) {
		return std::unexpected(PublisherRevocationExportError::Unwritable);
	}

	return target;
}
}
