#include "downloader/announce/ControlSwarmTorrent.h"

#include <libtorrent/hasher.hpp>

#include <format>

namespace wgrd::downloader {
std::vector<std::uint8_t> ControlSwarmTorrent::Payload_() {
	std::vector<std::uint8_t> payload(PAYLOAD_BYTES);

	for (std::size_t index = 0; index < PAYLOAD_BYTES; ++index) {
		payload[index] = static_cast<std::uint8_t>((index * 61 + 17) & 0xFF);
	}

	return payload;
}

ControlSwarmTorrent::Built ControlSwarmTorrent::Create() {
	Built built;

	built.payload = Payload_();
	built.name = std::string(FILE_NAME);

	libtorrent::hasher pieceHasher;
	pieceHasher.update(
		reinterpret_cast<const char*>(built.payload.data()),
		static_cast<int>(built.payload.size())
	);

	const libtorrent::sha1_hash piece = pieceHasher.final();

	const std::string prefix = std::format(
		"d4:infod6:lengthi{}e4:name{}:{}12:piece lengthi{}e6:pieces20:",
		PAYLOAD_BYTES,
		FILE_NAME.size(),
		FILE_NAME,
		PIECE_BYTES
	);

	built.bencoded.assign(prefix.begin(), prefix.end());

	for (const char value : piece) {
		built.bencoded.push_back(value);
	}

	built.bencoded.push_back('e');
	built.bencoded.push_back('e');

	return built;
}
}
