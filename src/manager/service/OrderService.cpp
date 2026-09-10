#include "manager/service/OrderService.h"

#include "manager/order/OrderFileGateway.h"
#include "manager/scan/ModFolderScanner.h"

#include <algorithm>
#include <utility>

namespace wgrd::manager {
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
	const auto stored = OrderFileGateway::Read(_installation.orderFile);
	if (stored.has_value()) {
		for (const domain::OrderEntry& entry : stored->Entries()) {
			refreshed.push_back(domain::OrderEntryView{entry.folder, true, false});
		}
	}
	for (const domain::OrderEntryView& previous : _entries) {
		if (previous.enabled) {
			continue;
		}
		const bool present = std::ranges::any_of(refreshed, [&previous](const domain::OrderEntryView& candidate) {
			return candidate.folder == previous.folder;
		});
		if (!present) {
			refreshed.push_back(domain::OrderEntryView{previous.folder, false, false});
		}
	}
	for (const domain::InstalledMod& mod : installed) {
		const bool listed = std::ranges::any_of(refreshed, [&mod](const domain::OrderEntryView& candidate) {
			return candidate.folder == mod.folder;
		});
		if (!listed) {
			refreshed.push_back(domain::OrderEntryView{mod.folder, false, true});
		}
	}
	if (OrderFileGateway::Serialize(ComposeOrder_(refreshed)) != _snapshot.filePreview) {
		_undo.clear();
	}
	_entries = std::move(refreshed);
	_snapshot.installed = installed;
	_snapshot.writable = true;
	Rebuild_();
	_message = stored.has_value() || stored.error() == OrderFileError::NotFound ? std::string() : "Load order unreadable";
}

void OrderService::SetEnabled(const domain::InstallFolder& folder, const bool enabled) {
	auto entries = _entries;
	const auto found = std::ranges::find_if(entries, [&folder](const domain::OrderEntryView& entry) {
		return entry.folder == folder;
	});
	if (found == entries.end()) {
		_message = "Mod entry missing";
		return;
	}
	if (found->enabled == enabled) {
		return;
	}
	found->enabled = enabled;
	const bool committed = Commit_(std::move(entries), true, true);
	(void)committed;
}

void OrderService::Move(const std::size_t fromIndex, const std::size_t toIndex) {
	if (fromIndex >= _entries.size() || toIndex >= _entries.size()) {
		_message = "Order position invalid";
		return;
	}
	if (fromIndex == toIndex) {
		return;
	}
	auto entries = _entries;
	const domain::OrderEntryView moved = entries[fromIndex];
	entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(fromIndex));
	entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(toIndex), moved);
	const bool committed = Commit_(std::move(entries), true, true);
	(void)committed;
}

void OrderService::SetChangeHandler(std::function<void()> handler) {
	_onChanged = std::move(handler);
}

void OrderService::Announce_() const {
	if (_onChanged) {
		_onChanged();
	}
}

std::vector<domain::OrderEntryView> OrderService::Expand_(const domain::LoadOrder& order) const {
	std::vector<domain::OrderEntryView> entries;
	for (const domain::OrderEntry& entry : order.Entries()) {
		entries.push_back(domain::OrderEntryView{entry.folder, entry.enabled, false});
	}
	for (const domain::InstalledMod& mod : _snapshot.installed) {
		const bool listed = std::ranges::any_of(entries, [&mod](const domain::OrderEntryView& candidate) {
			return candidate.folder == mod.folder;
		});
		if (!listed) {
			entries.push_back(domain::OrderEntryView{mod.folder, false, true});
		}
	}
	return entries;
}

bool OrderService::Apply(const domain::LoadOrder& order) {
	if (!Commit_(Expand_(order), false, false)) {
		return false;
	}
	_undo.clear();
	return true;
}

domain::LoadOrder OrderService::ComposeOrder_(const std::vector<domain::OrderEntryView>& views) {
	std::vector<domain::OrderEntry> entries;
	entries.reserve(views.size());
	for (const domain::OrderEntryView& entry : views) {
		entries.push_back(domain::OrderEntry{entry.folder, entry.enabled});
	}
	return domain::LoadOrder(std::move(entries));
}

bool OrderService::Commit_(std::vector<domain::OrderEntryView> entries, const bool remember, const bool announce) {
	const auto written = OrderFileGateway::Write(_installation.orderFile, ComposeOrder_(entries));
	if (!written.has_value()) {
		_snapshot.writable = false;
		_message = "Load order write failed";
		return false;
	}
	if (remember) {
		if (_undo.size() == HISTORY_LIMIT) {
			_undo.pop_front();
		}
		_undo.push_back(ComposeOrder_(_entries));
	}
	_entries = std::move(entries);
	_snapshot.writable = true;
	Rebuild_();
	_message = "Order saved";
	if (announce) {
		Announce_();
	}
	return true;
}

bool OrderService::CanUndo() const {
	return !_undo.empty();
}

bool OrderService::Undo() {
	if (_undo.empty()) {
		_message = "Nothing to undo";
		return false;
	}
	if (!Commit_(Expand_(_undo.back()), false, true)) {
		return false;
	}
	_undo.pop_back();
	_message = "Order change undone";
	return true;
}

std::string_view OrderService::LastMessage() const {
	return _message;
}

void OrderService::Rebuild_() {
	const domain::LoadOrder order = ComposeOrder_(_entries);
	for (domain::OrderEntryView& entry : _entries) {
		entry.present = std::ranges::any_of(_snapshot.installed, [&entry](const domain::InstalledMod& mod) {
			return mod.folder == entry.folder;
		});
	}
	_snapshot.entries = _entries;
	_snapshot.filePreview = OrderFileGateway::Serialize(order);
	_snapshot.enabledCount = order.EnabledCount();
	_snapshot.gameRoot = _installation.root.string();
	_snapshot.modsDirectory = _installation.modsDirectory.filename().string();
	_snapshot.orderFile = _installation.orderFile.string();
	_snapshot.located = _installation.IsUsable();
	_snapshot.writable = _snapshot.located && _snapshot.writable;
	_snapshot.annotations = _inspector.Inspect(order, _snapshot.installed);
}
}
