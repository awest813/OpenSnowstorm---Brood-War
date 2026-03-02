#ifndef BWGAME_SYNC_PROTOCOL_H
#define BWGAME_SYNC_PROTOCOL_H

// Sync protocol message identifiers for the OpenBW multiplayer
// synchronization layer.
//
// Wire format
// -----------
// Each message begins with a 1-byte message type tag drawn from the
// id_* constants below, followed by message-specific payload bytes
// (see sync_functions::syncer_t send_* methods in sync.h for encoding).
//
// Stability contract
// ------------------
// These integer values are part of the on-wire protocol.  They MUST NOT be
// reordered or renumbered across versions.  Add new message types only at
// the END of the enum, and update this comment with the version that
// introduced them.
//
//   id_client_uid      (0) – client announces its unique ID
//   id_client_frame    (1) – client reports its current frame number
//   id_occupy_slot     (2) – client requests / assigns a player slot
//   id_start_game      (3) – host signals that the game should start
//   id_game_info       (4) – host broadcasts lobby / game-info block
//   id_set_race        (5) – client selects a race
//   id_game_started    (6) – host confirms game has started
//   id_leave_game      (7) – client leaving mid-game
//   id_insync_check    (8) – periodic hash checkpoint for desync detection
//   id_create_unit     (9) – debug/cheat: create a unit
//   id_kill_unit      (10) – debug/cheat: kill a unit
//   id_remove_unit    (11) – debug/cheat: remove a unit
//   id_custom_action  (12) – arbitrary action payload (BWAPI / bot use)

namespace bwgame {

namespace sync_messages {

// Primary message type identifiers (1-byte wire tag).
enum {
	id_client_uid,       // 0
	id_client_frame,     // 1
	id_occupy_slot,      // 2
	id_start_game,       // 3
	id_game_info,        // 4
	id_set_race,         // 5
	id_game_started,     // 6
	id_leave_game,       // 7
	id_insync_check,     // 8
	id_create_unit,      // 9
	id_kill_unit,        // 10
	id_remove_unit,      // 11
	id_custom_action     // 12
};

// Special escape byte used to embed a game-started signal within a
// multiplexed client-frame stream (distinct from id_game_started above).
enum {
	id_game_started_escape = 0xdc
};

} // namespace sync_messages

} // namespace bwgame

#endif // BWGAME_SYNC_PROTOCOL_H
