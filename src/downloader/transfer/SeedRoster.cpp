#include "downloader/transfer/SeedRoster.h"

#include <utility>

namespace wgrd::downloader {
SeedRoster::SeedRoster()
	: _guard()
	, _seeded()
	, _entries() {}

std::size_t SeedRoster::PositionOf_(const std::string_view identifier) const {
	for (std::size_t position = 0; position < _seeded.size(); ++position) {
		if (_seeded[position].identifier == identifier) {
			return position;
		}
	}

	return _seeded.size();
}

bool SeedRoster::Contains(const std::string_view identifier) const {
	const std::scoped_lock lock(_guard);
	return PositionOf_(identifier) < _seeded.size();
}

bool SeedRoster::Add(SeededTorrent seeded, domain::SeedEntry entry) {
	const std::scoped_lock lock(_guard);

	if (PositionOf_(seeded.identifier) < _seeded.size()) {
		return false;
	}

	_seeded.push_back(std::move(seeded));
	_entries.push_back(std::move(entry));

	return true;
}

std::optional<RemovedSeed> SeedRoster::Remove(const std::string_view identifier) {
	const std::scoped_lock lock(_guard);

	const std::size_t position = PositionOf_(identifier);
	if (position >= _seeded.size()) {
		return std::nullopt;
	}

	RemovedSeed removed{
		std::move(_seeded[position].handle),
		std::move(_seeded[position].manifest),
		std::move(_seeded[position].modFolder),
		position < _entries.size() ? _entries[position].infoHash : std::string()
	};

	_seeded.erase(_seeded.begin() + static_cast<std::ptrdiff_t>(position));

	if (position < _entries.size()) {
		_entries.erase(_entries.begin() + static_cast<std::ptrdiff_t>(position));
	}

	return removed;
}

std::vector<SeedHandleView> SeedRoster::Handles() const {
	const std::scoped_lock lock(_guard);

	std::vector<SeedHandleView> views;
	views.reserve(_seeded.size());

	for (std::size_t position = 0; position < _seeded.size(); ++position) {
		views.push_back(SeedHandleView{
				_seeded[position].identifier,
				_seeded[position].handle,
				position < _entries.size() ? _entries[position].peers : 0
			}
		);
	}

	return views;
}

std::shared_ptr<libtorrent::torrent_handle> SeedRoster::HandleFor(
	const std::string_view identifier
) const {
	const std::scoped_lock lock(_guard);

	const std::size_t position = PositionOf_(identifier);
	if (position >= _seeded.size()) {
		return nullptr;
	}

	return _seeded[position].handle;
}

std::vector<domain::SeedEntry> SeedRoster::Snapshot() const {
	const std::scoped_lock lock(_guard);
	return _entries;
}

std::uint64_t SeedRoster::UploadedBytes() const {
	const std::scoped_lock lock(_guard);

	std::uint64_t total = 0;
	for (const domain::SeedEntry& entry : _entries) {
		total += entry.uploadedBytes;
	}

	return total;
}

void SeedRoster::Update(
	const std::string_view identifier,
	const bool seeding,
	const std::uint32_t peers,
	const std::uint64_t uploadedBytes
) {
	const std::scoped_lock lock(_guard);

	const std::size_t position = PositionOf_(identifier);
	if (position >= _entries.size()) {
		return;
	}

	_entries[position].seeding = seeding;
	_entries[position].peers = peers;
	_entries[position].uploadedBytes = uploadedBytes;
}
}
