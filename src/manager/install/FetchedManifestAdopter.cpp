#include "manager/install/FetchedManifestAdopter.h"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <vector>

namespace wgrd::manager {
FetchedManifestAdopter::FetchedManifestAdopter(
	const domain::IContentHasher& hasher,
	const domain::IManifestAuthenticator& authenticator,
	const domain::IManifestCodec& codec,
	const ManifestStore& store
)
	: _hasher(&hasher)
	, _authenticator(&authenticator)
	, _codec(&codec)
	, _store(&store) {}

std::optional<domain::ModManifest> FetchedManifestAdopter::Adopt(
	const domain::SignedAnnounce& announce,
	const std::filesystem::path& stagedManifest
) const {
	std::ifstream input(stagedManifest, std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	std::vector<std::uint8_t> sealed;
	sealed.assign(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);

	if (sealed.empty()) {
		return std::nullopt;
	}

	std::vector<std::byte> raw;
	raw.reserve(sealed.size());
	for (const std::uint8_t value : sealed) {
		raw.push_back(static_cast<std::byte>(value));
	}

	if (_hasher->Hash(raw) != announce.manifestDigest) {
		return std::nullopt;
	}

	const auto authenticated = _authenticator->Authenticate(sealed);
	if (!authenticated.has_value()) {
		return std::nullopt;
	}

	const auto decoded = _codec->Decode(authenticated->payload);
	if (!decoded.has_value()) {
		return std::nullopt;
	}

	if (decoded->Identifier() != announce.Identifier()
	    || decoded->Version() != announce.version) {
		return std::nullopt;
	}

	if (!_store->Save(announce.manifestDigest.ToHex(), sealed).has_value()) {
		return std::nullopt;
	}

	return *decoded;
}
}
