#include "manager/service/OrderService.h"

#include "manager/order/OrderFileGateway.h"
#include "manager/scan/ModFolderScanner.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::manager {
namespace {
	bool DirectoryAcceptsWrites(const std::filesystem::path& directory) {
		const std::filesystem::path probe = directory / "wgrdmm-write-probe.tmp";

		{
			std::ofstream output(probe, std::ios::binary | std::ios::trunc);
			if (!output) {
				return false;
			}
		}

		std::error_code removal;
		std::filesystem::remove(probe, removal);
		return true;
	}
}

OrderService::OrderService(domain::GameInstallation installation)
	: _installation(std::move(installation)) {
	Refresh();
}

const domain::OrderSnapshot& OrderService::Current() const {
	return _snapshot;
}

void OrderService::Refresh() {
	const std::vector<domain::InstalledMod> installed = ModFolderScanner::Scan(_installation.modsDirectory);

	std::vector<domain::OrderEntryView> refreshed;

	if (const auto stored = OrderFileGateway::Read(_installation.orderFile)) {
		for (const domain::OrderEntry& entry : stored->Entries()) {
			refreshed.push_back(domain::OrderEntryView{entry.folder, true, false});
		}
	}

	for (const domain::OrderEntryView& previous : _entries) {
		if (previous.enabled) {
			continue;
		}
		const bool alreadyPresent = std::ranges::any_of(refreshed, [&](const domain::OrderEntryView& candidate) {
				return candidate.folder == previous.folder;
			}
		);
		if (!alreadyPresent) {
			refreshed.push_back(domain::OrderEntryView{previous.folder, false, false});
		}
	}

	for (const domain::InstalledMod& mod : installed) {
		const bool listed = std::ranges::any_of(refreshed, [&](const domain::OrderEntryView& candidate) {
				return candidate.folder == mod.folder;
			}
		);
		if (!listed) {
			refreshed.push_back(domain::OrderEntryView{mod.folder, false, true});
		}
	}

	_entries = std::move(refreshed);


	_snapshot.installed = installed;
	Rebuild_();
}

void OrderService::SetEnabled(const domain::InstallFolder& folder, const bool enabled) {
	for (domain::OrderEntryView& entry : _entries) {
		if (entry.folder == folder) {
			entry.enabled = enabled;
			break;
		}
	}

	Persist_();
	Rebuild_();
	Announce_();
}

void OrderService::Move(const std::size_t fromIndex, const std::size_t toIndex) {
	if (fromIndex >= _entries.size() || toIndex >= _entries.size() || fromIndex == toIndex) {
		return;
	}

	const domain::OrderEntryView moved = _entries[fromIndex];
	_entries.erase(_entries.begin() + static_cast<std::ptrdiff_t>(fromIndex));
	_entries.insert(_entries.begin() + static_cast<std::ptrdiff_t>(toIndex), moved);

	Persist_();
	Rebuild_();
	Announce_();
}

void OrderService::SetChangeHandler(std::function<void()> handler) {
	_onChanged = std::move(handler);
}

void OrderService::Announce_() const {
	if (_onChanged) {
		_onChanged();
	}
}

bool OrderService::Apply(const domain::LoadOrder& order) {
	std::vector<domain::OrderEntryView> applied;
	applied.reserve(_entries.size());

	for (const domain::OrderEntry& entry : order.Entries()) {
		applied.push_back(domain::OrderEntryView{entry.folder, entry.enabled, false});
	}

	for (const domain::InstalledMod& mod : _snapshot.installed) {
		const bool listed = std::ranges::any_of(applied, [&](const domain::OrderEntryView& candidate) {
				return candidate.folder == mod.folder;
			}
		);

		if (!listed) {
			applied.push_back(domain::OrderEntryView{mod.folder, false, true});
		}
	}

	_entries = std::move(applied);

	Persist_();
	Rebuild_();

	return _snapshot.writable;
}

domain::LoadOrder OrderService::ComposeOrder_() const {
	std::vector<domain::OrderEntry> entries;
	entries.reserve(_entries.size());

	for (const domain::OrderEntryView& entry : _entries) {
		entries.push_back(domain::OrderEntry{entry.folder, entry.enabled});
	}

	return domain::LoadOrder(std::move(entries));
}

void OrderService::Persist_() {
	const auto written = OrderFileGateway::Write(_installation.orderFile, ComposeOrder_());
	_snapshot.writable = written.has_value();
}

void OrderService::Rebuild_() {
	const domain::LoadOrder order = ComposeOrder_();

	for (domain::OrderEntryView& entry : _entries) {
		const auto match = std::ranges::find_if(_snapshot.installed, [&](const domain::InstalledMod& mod) {
				return mod.folder == entry.folder;
			}
		);

		entry.present = match != _snapshot.installed.end();
	}

	_snapshot.entries = _entries;
	_snapshot.filePreview = OrderFileGateway::Serialize(order);
	_snapshot.enabledCount = order.EnabledCount();
	_snapshot.gameRoot = _installation.root.string();
	_snapshot.modsDirectory = _installation.modsDirectory.filename().string();
	_snapshot.orderFile = _installation.orderFile.string();
	_snapshot.located = _installation.IsUsable();
	_snapshot.writable = _snapshot.located && DirectoryAcceptsWrites(_installation.modsDirectory);

	_snapshot.annotations = _inspector.Inspect(order, _snapshot.installed);
}
}
