#include "downloader/storage/ResumeAttestation.h"

#include <libtorrent/add_torrent_params.hpp>

namespace wgrd::downloader {
bool ResumeAttestation::AttestsPieces(
	const libtorrent::add_torrent_params* resumeData,
	const int pieceCount
) {
	if (resumeData == nullptr || pieceCount <= 0) {
		return false;
	}

	if (resumeData->have_pieces.size() < pieceCount) {
		return false;
	}

	for (libtorrent::piece_index_t piece(0); piece < libtorrent::piece_index_t(pieceCount); ++piece) {
		if (!resumeData->have_pieces.get_bit(piece)) {
			return false;
		}
	}

	return true;
}
}
