#pragma once

#include "domain/types/identity/PublisherIdentity.h"
#include "domain/types/identity/Signature.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace wgrd::manager {
enum class PublisherRevocationExportError {
	AlreadyPresent, Unwritable
};

class PublisherRevocationExporter {
public:
	static constexpr std::string_view FILE_EXTENSION = ".json";
	static constexpr std::string_view DEFAULT_REASON = "publisher retired key";

	[[nodiscard]] static std::string FileNameFor(const domain::PublisherIdentity& identity);

	[[nodiscard]] static std::expected<std::filesystem::path, PublisherRevocationExportError> Export(
		const domain::PublisherIdentity& identity,
		const domain::Signature& signature,
		std::string_view revokedAt,
		std::string_view reason,
		const std::filesystem::path& destinationFolder,
		bool replaceExisting
	);

private:
	PublisherRevocationExporter() = delete;
};
}
