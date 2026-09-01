#include "downloader/announce/AnnounceGossipPeerPlugin.h"

#include "downloader/announce/AnnounceWireCodec.h"

#include <libtorrent/bdecode.hpp>
#include <libtorrent/entry.hpp>

#include <utility>

namespace wgrd::downloader {
namespace {
	constexpr int EXTENDED_MESSAGE = 20;
	constexpr std::size_t LENGTH_PREFIX_BYTES = 4;
	constexpr std::size_t EXTENSION_HEADER_BYTES = 2;

	std::span<const std::uint8_t> AsBytes(const libtorrent::span<const char> body) {
		return std::span<const std::uint8_t>(
			reinterpret_cast<const std::uint8_t*>(body.data()),
			static_cast<std::size_t>(body.size())
		);
	}
}

AnnounceGossipPeerPlugin::AnnounceGossipPeerPlugin(
	libtorrent::peer_connection_handle connection,
	AnnounceExchange& exchange
)
	: _connection(std::move(connection))
	, _exchange(&exchange)
	, _remoteMessageId(0)
	, _counted(false)
	, _wants()
	, _lastOffered()
	, _lastOffer()
	, _lastHoldingsPoll() {}

AnnounceGossipPeerPlugin::~AnnounceGossipPeerPlugin() {
	if (_counted) {
		_exchange->PeerClosed();
	}
}

libtorrent::string_view AnnounceGossipPeerPlugin::type() const {
	return libtorrent::string_view(EXTENSION_NAME.data(), EXTENSION_NAME.size());
}

std::string AnnounceGossipPeerPlugin::KeyOf_(const domain::AnnounceSummary& summary) {
	return summary.Identifier();
}

void AnnounceGossipPeerPlugin::PenaliseAndDisconnect_() {
	_exchange->Penalise(PeerKey_());
	_connection.disconnect(
		libtorrent::error_code(libtorrent::errors::invalid_message),
		libtorrent::operation_t::bittorrent
	);
}

std::string AnnounceGossipPeerPlugin::PeerKey_() const {
	const libtorrent::tcp::endpoint endpoint = _connection.remote();

	return endpoint.address().to_string();
}

void AnnounceGossipPeerPlugin::add_handshake(libtorrent::entry& handshake) {
	libtorrent::entry::dictionary_type& messages =
			handshake["m"].dict();

	messages[std::string(EXTENSION_NAME)] = libtorrent::entry(LOCAL_MESSAGE_ID);

	handshake[std::string(REVISION_KEY)] = libtorrent::entry(LOCAL_REVISION);

	const std::uint16_t listening = _exchange->ListenPort();
	if (listening != 0) {
		handshake["p"] = libtorrent::entry(static_cast<std::int64_t>(listening));
	}
}

bool AnnounceGossipPeerPlugin::on_extension_handshake(const libtorrent::bdecode_node& handshake) {
	if (handshake.type() != libtorrent::bdecode_node::dict_t) {
		return false;
	}

	const libtorrent::bdecode_node messages = handshake.dict_find_dict("m");
	if (!messages) {
		return false;
	}

	const std::int64_t advertised = messages.dict_find_int_value(
		libtorrent::string_view(EXTENSION_NAME.data(), EXTENSION_NAME.size()),
		-1
	);

	if (advertised <= 0 || advertised > 255) {
		return false;
	}

	_remoteMessageId = static_cast<int>(advertised);

	_remoteRevision = handshake.dict_find_int_value(
		libtorrent::string_view(REVISION_KEY.data(), REVISION_KEY.size()),
		0
	);

	const std::int64_t listening = handshake.dict_find_int_value("p", 0);

	if (listening > 0 && listening <= 65535) {
		_exchange->NotePeer(PeerKey_(), static_cast<std::uint16_t>(listening));
	}

	_exchange->PeerOpened();
	_counted = true;

	_seenRefresh = _exchange->RefreshGeneration();

	SendOffer_(_exchange->Holdings());

	return true;
}

void AnnounceGossipPeerPlugin::Send_(const std::vector<std::uint8_t>& payload) {
	if (_remoteMessageId == 0 || payload.empty()) {
		return;
	}

	if (payload.size() > AnnounceWireCodec::MAXIMUM_MESSAGE_BYTES) {
		return;
	}

	const std::uint32_t length =
			static_cast<std::uint32_t>(EXTENSION_HEADER_BYTES + payload.size());

	std::vector<char> frame;
	frame.reserve(LENGTH_PREFIX_BYTES + length);

	frame.push_back(static_cast<char>((length >> 24) & 0xFF));
	frame.push_back(static_cast<char>((length >> 16) & 0xFF));
	frame.push_back(static_cast<char>((length >> 8) & 0xFF));
	frame.push_back(static_cast<char>(length & 0xFF));

	frame.push_back(EXTENDED_MESSAGE);
	frame.push_back(static_cast<char>(_remoteMessageId));

	for (const std::uint8_t value : payload) {
		frame.push_back(static_cast<char>(value));
	}

	_connection.send_buffer(frame.data(), static_cast<int>(frame.size()));
}

void AnnounceGossipPeerPlugin::SendOffer_(std::vector<domain::AnnounceSummary> holdings) {
	Send_(AnnounceWireCodec::EncodeOffer(holdings));

	_exchange->CountOfferSent();
	_lastOffered = std::move(holdings);
	_lastOffer = std::chrono::steady_clock::now();
}

void AnnounceGossipPeerPlugin::HandleOffer_(const libtorrent::span<const char> body) {
	const auto offered = AnnounceWireCodec::DecodeOffer(AsBytes(body));
	if (!offered.has_value()) {
		PenaliseAndDisconnect_();
		return;
	}

	_exchange->CountOfferReceived();

	const std::vector<domain::AnnounceSummary> wanted = _exchange->Missing(*offered);
	if (wanted.empty()) {
		return;
	}

	const auto now = OutstandingWantTracker::Clock::now();

	_wants.Prune(now);

	std::vector<domain::AnnounceSummary> tracked;
	tracked.reserve(wanted.size());

	for (const domain::AnnounceSummary& summary : wanted) {
		if (!_wants.Track(KeyOf_(summary), now)) {
			continue;
		}

		tracked.push_back(summary);
	}

	if (tracked.empty()) {
		return;
	}

	Send_(AnnounceWireCodec::EncodeWant(tracked));
}

void AnnounceGossipPeerPlugin::HandleWant_(const libtorrent::span<const char> body) {
	const auto wanted = AnnounceWireCodec::DecodeWant(AsBytes(body));
	if (!wanted.has_value()) {
		PenaliseAndDisconnect_();
		return;
	}

	for (const domain::AnnounceSummary& summary : *wanted) {
		const auto record = _exchange->Serve(summary);
		if (!record.has_value()) {
			continue;
		}

		Send_(AnnounceWireCodec::EncodeRecord(*record));
	}
}

void AnnounceGossipPeerPlugin::HandleRecord_(const libtorrent::span<const char> body) {
	const auto record = AnnounceWireCodec::DecodeRecord(AsBytes(body));
	if (!record.has_value()) {
		PenaliseAndDisconnect_();
		return;
	}

	const auto identity = AnnounceWireCodec::RecordSummary(*record);
	if (!identity.has_value()) {
		PenaliseAndDisconnect_();
		return;
	}

	if (!_wants.Redeem(KeyOf_(*identity))) {
		PenaliseAndDisconnect_();
		return;
	}

	const std::string peer = PeerKey_();

	if (!_exchange->Ingest(peer, *record)) {
		if (_exchange->PeerBlocked(peer)) {
			_connection.disconnect(
				libtorrent::error_code(libtorrent::errors::invalid_message),
				libtorrent::operation_t::bittorrent
			);
		}
	}
}

bool AnnounceGossipPeerPlugin::on_extended(
	const int length,
	const int message,
	const libtorrent::span<const char> body
) {
	if (message != LOCAL_MESSAGE_ID) {
		return false;
	}

	if (length > static_cast<int>(AnnounceWireCodec::MAXIMUM_MESSAGE_BYTES)) {
		PenaliseAndDisconnect_();
		return true;
	}

	if (static_cast<int>(body.size()) < length) {
		return true;
	}

	const auto kind = AnnounceWireCodec::MessageOf(AsBytes(body));
	if (!kind.has_value()) {
		return true;
	}

	switch (*kind) {
		case AnnounceWireMessage::Offer:
			HandleOffer_(body);
			break;
		case AnnounceWireMessage::Want:
			HandleWant_(body);
			break;
		case AnnounceWireMessage::Record:
			HandleRecord_(body);
			break;
		case AnnounceWireMessage::Ask:
			SendOffer_(_exchange->Holdings());
			break;
	}

	return true;
}

void AnnounceGossipPeerPlugin::tick() {
	if (_remoteMessageId == 0) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();

	if (now - _lastHoldingsPoll < HOLDINGS_POLL_INTERVAL) {
		return;
	}

	_lastHoldingsPoll = now;

	_wants.Prune(now);

	const std::uint64_t generation = _exchange->RefreshGeneration();

	if (generation != _seenRefresh) {
		_seenRefresh = generation;

		if (_remoteRevision >= ASK_REVISION) {
			Send_(AnnounceWireCodec::EncodeAsk());
		}

		SendOffer_(_exchange->Holdings());

		return;
	}

	std::vector<domain::AnnounceSummary> holdings = _exchange->Holdings();

	const bool changed = holdings != _lastOffered;
	const bool overdue = now - _lastOffer >= REOFFER_INTERVAL;

	if (!changed && !overdue) {
		return;
	}

	SendOffer_(std::move(holdings));
}
}
