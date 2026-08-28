#pragma once

#include "domain/interfaces/trust/IKeyRegistry.h"
#include "domain/types/identity/RegisteredKey.h"
#include "domain/types/identity/RevocationCertificate.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <vector>

namespace wgrd::manager {
class DirectoryKeyRegistry final : public domain::IKeyRegistry {
public:
	static constexpr std::string_view KEYS_FOLDER = "keys";
	static constexpr std::string_view REVOKED_FOLDER = "revoked";

	explicit DirectoryKeyRegistry(std::filesystem::path registryFolder);

	~DirectoryKeyRegistry() override;

	std::size_t Reload() override;

	[[nodiscard]] std::optional<domain::RegisteredKey> Find(
		const domain::PublisherFingerprint& fingerprint
	) const override;

	[[nodiscard]] bool IsUsable(const domain::PublisherFingerprint& fingerprint) const override;

	[[nodiscard]] std::size_t Count() const override;

private:
	[[nodiscard]] std::vector<domain::RegisteredKey> ReadKeys_() const;

	[[nodiscard]] std::vector<domain::RevocationCertificate> ReadRevocations_() const;

	static void ApplyRevocations_(
		std::vector<domain::RegisteredKey>& keys,
		const std::vector<domain::RevocationCertificate>& certificates
	);

	[[nodiscard]] static std::optional<domain::RegisteredKey> ReadKeyFile_(
		const std::filesystem::path& path
	);

	std::filesystem::path _registryFolder;
	mutable std::mutex _guard;
	std::vector<domain::RegisteredKey> _keys;
};
}
