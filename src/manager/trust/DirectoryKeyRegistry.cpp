#include "manager/trust/DirectoryKeyRegistry.h"

#include "manager/trust/FingerprintDeriver.h"
#include "manager/trust/KeyRegistry.h"
#include "manager/trust/RevocationCertificateReader.h"
#include "manager/trust/RevocationVerifier.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::manager {
namespace {
	using Json = nlohmann::json;

	Json ReadDocument(const std::filesystem::path& path) {
		std::ifstream input(path, std::ios::binary);
		if (!input) {
			return Json();
		}

		return Json::parse(input, nullptr, false);
	}
}

DirectoryKeyRegistry::DirectoryKeyRegistry(std::filesystem::path registryFolder)
	: _registryFolder(std::move(registryFolder))
	, _guard()
	, _keys() {
	Reload();
}

DirectoryKeyRegistry::~DirectoryKeyRegistry() = default;

std::optional<domain::RegisteredKey> DirectoryKeyRegistry::ReadKeyFile_(
	const std::filesystem::path& path
) {
	const Json document = ReadDocument(path);
	if (document.is_discarded() || !document.is_object()) {
		return std::nullopt;
	}

	if (!document.contains("fingerprint") || !document.contains("publicKey")) {
		return std::nullopt;
	}

	const Json& fingerprintField = document.at("fingerprint");
	const Json& publicKeyField = document.at("publicKey");

	if (!fingerprintField.is_string() || !publicKeyField.is_string()) {
		return std::nullopt;
	}

	const auto fingerprint = domain::PublisherFingerprint::FromHex(fingerprintField.get<std::string>());
	const auto publicKey = domain::PublicKey::FromHex(publicKeyField.get<std::string>());

	if (!fingerprint.has_value() || !publicKey.has_value()) {
		return std::nullopt;
	}

	if (FingerprintDeriver::Derive(*publicKey) != *fingerprint) {
		return std::nullopt;
	}

	std::string publisher;
	if (document.contains("publisher") && document.at("publisher").is_string()) {
		publisher = document.at("publisher").get<std::string>();
	}

	return domain::RegisteredKey{*fingerprint, *publicKey, std::move(publisher), false};
}

std::vector<domain::RegisteredKey> DirectoryKeyRegistry::ReadKeys_() const {
	std::vector<domain::RegisteredKey> keys;

	std::error_code failure;
	const std::filesystem::path folder = _registryFolder / KEYS_FOLDER;
	if (!std::filesystem::is_directory(folder, failure)) {
		return keys;
	}

	std::filesystem::directory_iterator walker(folder, failure);
	if (failure) {
		return keys;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != ".json") {
			continue;
		}

		auto key = ReadKeyFile_(walker->path());
		if (key.has_value()) {
			keys.push_back(std::move(*key));
		}
	}

	return keys;
}

std::vector<domain::RevocationCertificate> DirectoryKeyRegistry::ReadRevocations_() const {
	std::vector<domain::RevocationCertificate> certificates;

	std::error_code failure;
	const std::filesystem::path folder = _registryFolder / REVOKED_FOLDER;
	if (!std::filesystem::is_directory(folder, failure)) {
		return certificates;
	}

	std::filesystem::directory_iterator walker(folder, failure);
	if (failure) {
		return certificates;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != ".json") {
			continue;
		}

		auto certificate = RevocationCertificateReader::FromFile(walker->path());
		if (certificate.has_value()) {
			certificates.push_back(std::move(*certificate));
		}
	}

	return certificates;
}

void DirectoryKeyRegistry::ApplyRevocations_(
	std::vector<domain::RegisteredKey>& keys,
	const std::vector<domain::RevocationCertificate>& certificates
) {
	for (domain::RegisteredKey& key : keys) {
		if (key.revoked) {
			continue;
		}

		const auto certificate = std::ranges::find_if(
			certificates,
			[&key](const domain::RevocationCertificate& candidate) {
				return candidate.fingerprint == key.fingerprint;
			}
		);

		if (certificate == certificates.end()) {
			continue;
		}

		key.revoked = RevocationVerifier::Accepts(*certificate, key.publicKey);
	}
}

std::size_t DirectoryKeyRegistry::Reload() {
	std::vector<domain::RegisteredKey> merged = KeyRegistry::Baseline();

	for (domain::RegisteredKey& key : ReadKeys_()) {
		const auto existing = std::ranges::find_if(merged, [&key](const domain::RegisteredKey& candidate) {
				return candidate.fingerprint == key.fingerprint;
			}
		);

		if (existing == merged.end()) {
			merged.push_back(std::move(key));
		}
	}

	ApplyRevocations_(merged, ReadRevocations_());

	const std::scoped_lock lock(_guard);

	_keys = std::move(merged);

	return _keys.size();
}

std::optional<domain::RegisteredKey> DirectoryKeyRegistry::Find(
	const domain::PublisherFingerprint& fingerprint
) const {
	const std::scoped_lock lock(_guard);

	const auto match = std::ranges::find_if(_keys, [&fingerprint](const domain::RegisteredKey& key) {
			return key.fingerprint == fingerprint;
		}
	);

	if (match == _keys.end()) {
		return std::nullopt;
	}

	return *match;
}

bool DirectoryKeyRegistry::IsUsable(const domain::PublisherFingerprint& fingerprint) const {
	const auto key = Find(fingerprint);
	return key.has_value() && !key->revoked;
}

std::size_t DirectoryKeyRegistry::Count() const {
	const std::scoped_lock lock(_guard);
	return _keys.size();
}
}
