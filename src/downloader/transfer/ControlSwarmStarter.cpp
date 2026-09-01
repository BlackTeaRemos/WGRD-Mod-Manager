#include "downloader/transfer/ControlSwarmStarter.h"

#include "downloader/announce/ControlSwarmTorrent.h"
#include "downloader/transfer/TrackerList.h"

#include <libtorrent/error_code.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <fstream>
#include <system_error>

namespace wgrd::downloader {
std::optional<PreparedControlTorrent> ControlSwarmStarter::Prepare(
	ChunkLocator& locator,
	const std::filesystem::path& dataDirectory
) {
	const ControlSwarmTorrent::Built control = ControlSwarmTorrent::Create();

	std::error_code creating;
	std::filesystem::create_directories(dataDirectory, creating);

	const std::filesystem::path payloadPath = dataDirectory / control.name;

	{
		std::ofstream output(payloadPath, std::ios::binary | std::ios::trunc);
		if (!output) {
			return std::nullopt;
		}

		output.write(
			reinterpret_cast<const char*>(control.payload.data()),
			static_cast<std::streamsize>(control.payload.size())
		);
	}

	if (!locator.RegisterFile(control.name, payloadPath, 0, control.payload.size())) {
		return std::nullopt;
	}

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(
			control.bencoded.data(),
			static_cast<std::ptrdiff_t>(control.bencoded.size())
		),
		parsing,
		libtorrent::load_torrent_limits{}
	);

	if (parsing || parameters.ti == nullptr) {
		return std::nullopt;
	}

	parameters.save_path = dataDirectory.string();
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	TrackerList::Apply(parameters);

	PreparedControlTorrent prepared{
		std::move(parameters),
		false,
		std::string()
	};

	prepared.hasV2 = prepared.parameters.ti->info_hashes().has_v2();
	prepared.infoHash =
			libtorrent::aux::to_hex(prepared.parameters.ti->info_hashes().v2.to_string());

	return prepared;
}
}
