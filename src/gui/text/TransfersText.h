#pragma once

#include <string_view>

namespace wgrd::gui::text::transfers {
constexpr std::string_view TITLE = "TRANSFERS";
constexpr std::string_view SUBTITLE = "chunk sets served from installed mods";

constexpr std::string_view SWARM = "SWARM";
constexpr std::string_view LISTENER = "LISTENER";
constexpr std::string_view GOSSIP = "ANNOUNCE GOSSIP";
constexpr std::string_view SEEDING = "SEEDING";
constexpr std::string_view DOWNLOADS = "DOWNLOADS";

constexpr std::string_view OFFLINE = "offline";
constexpr std::string_view DHT_RUNNING = "dht running";
constexpr std::string_view DHT_STARTING = "dht starting";

constexpr std::string_view GOSSIP_UNAVAILABLE = "gossip unavailable";
constexpr std::string_view SEEDING_UNAVAILABLE = "seeding unavailable";
constexpr std::string_view INSTALL_UNAVAILABLE = "installing unavailable";
constexpr std::string_view NOTHING_HELD = "nothing";
constexpr std::string_view IDLE = "empty";
constexpr std::string_view NOTHING_TO_MOVE = "nothing to move";

constexpr std::string_view PAUSE = "pause";
constexpr std::string_view RESUME = "resume";
constexpr std::string_view CANCEL = "cancel";

constexpr std::string_view PILL_SEEDING = "SEEDING";
constexpr std::string_view PILL_STARTING = "STARTING";
constexpr std::string_view CONTROL_OK = "ok";
constexpr std::string_view CONTROL_NONE = "none";

constexpr std::string_view DHT_NODES_FORMAT = "{} dht nodes";
constexpr std::string_view PORT_FORMAT = "port {}";
constexpr std::string_view PEERS_CONTROL_FORMAT = "{} peers {} control";
constexpr std::string_view OFFERS_FORMAT = "offers {} out {} in";
constexpr std::string_view RECORDS_FORMAT = "records {} out {} in";
constexpr std::string_view ACCEPTED_FORMAT = "{} accepted";
constexpr std::string_view REJECTED_FORMAT = "{} rejected";
constexpr std::string_view THROTTLED_FORMAT = "{} throttled {} bad";
constexpr std::string_view CONTROL_FORMAT = "control {} state {} dials {}";
constexpr std::string_view PEER_DROP_PREFIX = "local peer drop ";
constexpr std::string_view TRANSFER_FAULT_PREFIX = "transfer fault ";
constexpr std::string_view DISK_FAULT_FORMAT = "disk {} read {} write";
constexpr std::string_view SEEDING_HEADING_FORMAT = "SEEDING - {} MODS - {} UPLOADED";
constexpr std::string_view CHUNKS_FORMAT = "{} chunks";
constexpr std::string_view PEERS_FORMAT = "{} peers";
constexpr std::string_view BAD_DATA_FORMAT = "{} bad {} banned";
constexpr std::string_view TO_FETCH_FORMAT = "{} to fetch";
constexpr std::string_view REUSED_FORMAT = "{} reused";
constexpr std::string_view MOVED_FORMAT = "{} of {}";
}
