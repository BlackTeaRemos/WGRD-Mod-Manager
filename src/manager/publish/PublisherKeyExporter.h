#pragma once

#include "domain/types/identity/PublisherIdentity.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace wgrd::manager {
enum class PublisherKeyExportError {
	AlreadyPresent, Unwritable, PublisherRejected
};

class PublisherKeyExporter {
public:
	static constexpr std::string_view FILE_EXTENSION = ".json";

	[[nodiscard]] static std::string FileNameFor(const domain::PublisherIdentity& identity);

	[[nodiscard]] static std::expected<std::filesystem::path, PublisherKeyExportError> Export(
		const domain::PublisherIdentity& identity,
		std::string_view publisher,
		std::string_view addedAt,
		const std::filesystem::path& destinationFolder,
		bool replaceExisting
	);

private:
	PublisherKeyExporter() = delete;
};
}
