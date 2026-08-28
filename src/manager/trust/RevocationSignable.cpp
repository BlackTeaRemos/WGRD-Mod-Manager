#include "manager/trust/RevocationSignable.h"

#include <string>

namespace wgrd::manager {
std::vector<std::uint8_t> RevocationSignable::Bytes(
	const std::string_view fingerprint,
	const std::string_view revokedAt,
	const std::string_view reason
) {
	std::string joined;
	joined.append(PREFIX);
	joined.append(SEPARATOR);
	joined.append(fingerprint);
	joined.append(SEPARATOR);
	joined.append(revokedAt);
	joined.append(SEPARATOR);
	joined.append(reason);

	return std::vector<std::uint8_t>(joined.begin(), joined.end());
}
}
