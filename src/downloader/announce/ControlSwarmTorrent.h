#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::downloader {
class ControlSwarmTorrent {
public:
	static constexpr std::string_view FILE_NAME = "wgrd-control-1";
	static constexpr std::size_t PAYLOAD_BYTES = 32;
	static constexpr std::int32_t PIECE_BYTES = 16384;

	struct Built {
		std::vector<char> bencoded;
		std::vector<std::uint8_t> payload;
		std::string name;
	};

	[[nodiscard]] static Built Create();

private:
	ControlSwarmTorrent() = delete;

	[[nodiscard]] static std::vector<std::uint8_t> Payload_();
};
}
