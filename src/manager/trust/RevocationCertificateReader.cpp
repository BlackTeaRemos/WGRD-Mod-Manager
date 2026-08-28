#include "manager/trust/RevocationCertificateReader.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace wgrd::manager {
namespace {
	using Json = nlohmann::json;

	std::optional<std::string> BoundedString(const Json& document, const std::string_view field) {
		const std::string name(field);

		if (!document.contains(name) || !document.at(name).is_string()) {
			return std::nullopt;
		}

		std::string value = document.at(name).get<std::string>();
		if (value.size() > RevocationCertificateReader::FIELD_LIMIT) {
			return std::nullopt;
		}

		return value;
	}
}

std::optional<domain::RevocationCertificate> RevocationCertificateReader::FromDocument(
	const std::string_view document
) {
	const Json parsed = Json::parse(document.begin(), document.end(), nullptr, false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		return std::nullopt;
	}

	const std::optional<std::string> fingerprintText = BoundedString(parsed, "fingerprint");
	const std::optional<std::string> revokedAt = BoundedString(parsed, "revokedAt");
	const std::optional<std::string> reason = BoundedString(parsed, "reason");
	const std::optional<std::string> signatureText = BoundedString(parsed, "signature");

	if (!fingerprintText.has_value() || !revokedAt.has_value()) {
		return std::nullopt;
	}

	if (!reason.has_value() || !signatureText.has_value()) {
		return std::nullopt;
	}

	const auto fingerprint = domain::PublisherFingerprint::FromHex(*fingerprintText);
	const auto signature = domain::Signature::FromHex(*signatureText);

	if (!fingerprint.has_value() || !signature.has_value()) {
		return std::nullopt;
	}

	return domain::RevocationCertificate{*fingerprint, *revokedAt, *reason, *signature};
}

std::optional<domain::RevocationCertificate> RevocationCertificateReader::FromFile(
	const std::filesystem::path& path
) {
	std::error_code failure;

	const std::uintmax_t size = std::filesystem::file_size(path, failure);
	if (failure || size > DOCUMENT_LIMIT) {
		return std::nullopt;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	const std::string document(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	return FromDocument(document);
}
}
