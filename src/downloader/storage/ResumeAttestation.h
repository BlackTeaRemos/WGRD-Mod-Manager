#pragma once

#include <libtorrent/add_torrent_params.hpp>

namespace wgrd::downloader {
class ResumeAttestation {
public:
	[[nodiscard]] static bool AttestsPieces(
		const libtorrent::add_torrent_params* resumeData,
		int pieceCount
	);
};
}
