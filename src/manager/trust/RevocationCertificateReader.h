#pragma once

#include "domain/types/identity/RevocationCertificate.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::manager {
class RevocationCertificateReader {
public:
	static constexpr std::size_t FIELD_LIMIT = 128;
	static constexpr std::uintmax_t DOCUMENT_LIMIT = 8192;

	[[nodiscard]] static std::optional<domain::RevocationCertificate> FromFile(
		const std::filesystem::path& path
	);

	[[nodiscard]] static std::optional<domain::RevocationCertificate> FromDocument(
		std::string_view document
	);

private:
	RevocationCertificateReader() = delete;
};
}
