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

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <array>

namespace bwgame {

// ---------------------------------------------------------------------------
// Protocol version
// ---------------------------------------------------------------------------
//
// Bump sync_protocol_version when wire-incompatible changes are made to sync
// message semantics or layout.  Peers MUST compare versions during handshake
// to detect incompatible builds before gameplay begins.
//
// Compatibility policy
// --------------------
// - A peer that reports a version LOWER than sync_protocol_min_peer_version
//   must be rejected; it is too old to safely interoperate.
// - A peer that reports a version HIGHER than sync_protocol_version may be
//   accepted for the current session but should emit a diagnostic warning,
//   since the remote end may use message types or semantics unknown to this
//   build.
// - Version numbers are monotonically increasing unsigned integers; they do
//   not encode major/minor/patch components.
//
// Version history:
//   1 – initial versioned protocol (Phase 2 BW compatibility overhaul):
//       desync_report struct, richer insync hash (frame + owner + order),
//       patrol (action 29) and land (action 36) actions.

static constexpr uint32_t sync_protocol_version = 1;

// Oldest peer version that this build will accept during handshake.
// Raise this when a past version is known to produce incorrect behaviour.
static constexpr uint32_t sync_protocol_min_peer_version = 1;

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

// ---------------------------------------------------------------------------
// Structured desync report
// ---------------------------------------------------------------------------
//
// Emitted on the first insync hash mismatch.  Consumers (logs, CI harnesses,
// replay tools) can inspect desync_state::desync_reports to narrow down the
// cause without attaching a debugger.
//
// Fields
// ------
//   local_frame         – st.current_frame at the moment of detection.
//   hash_index          – which slot in the rolling hash ring was checked.
//   expected_hash       – the hash computed locally.
//   received_hash       – the hash reported by the diverging peer.
//   client_local_id     – internal client ID of the diverging peer (-1 if
//                         unknown, e.g. when the report is self-generated).
//   client_player_slot  – BW player-slot index of the peer (-1 if unknown).
//   recent_actions      – ring buffer of the last up-to-16 actions executed
//                         locally before the mismatch was detected; most-
//                         recent entry is at index (recent_actions_tail - 1)
//                         wrapping around recent_actions_capacity.

struct recent_action_entry {
	int frame = -1;
	int owner = -1;
	uint8_t action_id = 0;
};

struct desync_report {
	static constexpr size_t recent_actions_capacity = 16;

	int local_frame = -1;
	uint8_t hash_index = 0;
	uint32_t expected_hash = 0;
	uint32_t received_hash = 0;
	int client_local_id = -1;
	int client_player_slot = -1;

	// Ring-buffer of the most recent actions executed locally.
	// recent_actions_count  – how many valid entries are present (0-16).
	// recent_actions_tail   – index of the next slot to write (newest entry
	//                         is at [(tail - 1 + capacity) % capacity]).
	std::array<recent_action_entry, recent_actions_capacity> recent_actions{};
	size_t recent_actions_count = 0;
	size_t recent_actions_tail = 0;

	// Push one action entry into the ring buffer.
	void push_recent_action(int frame, int owner, uint8_t action_id) {
		recent_actions[recent_actions_tail] = {frame, owner, action_id};
		recent_actions_tail = (recent_actions_tail + 1) % recent_actions_capacity;
		if (recent_actions_count < recent_actions_capacity) ++recent_actions_count;
	}
};

// ---------------------------------------------------------------------------
// Desync report formatter
// ---------------------------------------------------------------------------
//
// Writes a human-readable summary of all accumulated desync reports to the
// FILE* sink (pass stderr for CI consumption, or a log file for persistent
// diagnostics).
//
// Output format (one block per report, machine-parseable by key: value):
//
//   [desync] report #N
//     local_frame        : <frame>
//     hash_index         : <index>
//     expected_hash      : 0x<hex>
//     received_hash      : 0x<hex>
//     client_local_id    : <id>
//     client_player_slot : <slot>
//     recent_actions     : <count> entries (oldest first)
//       [i] frame=<f> owner=<p> action=0x<id>
//
// Returns the number of reports written (0 if the vector is empty).
inline int write_desync_reports(
    FILE* sink,
    const desync_report* reports,
    size_t count)
{
	if (!sink || count == 0) return 0;

	for (size_t i = 0; i < count; ++i) {
		const desync_report& r = reports[i];
		fprintf(sink,
			"[desync] report #%zu\n"
			"  local_frame        : %d\n"
			"  hash_index         : %u\n"
			"  expected_hash      : 0x%08x\n"
			"  received_hash      : 0x%08x\n"
			"  client_local_id    : %d\n"
			"  client_player_slot : %d\n",
			i,
			r.local_frame,
			(unsigned)r.hash_index,
			(unsigned)r.expected_hash,
			(unsigned)r.received_hash,
			r.client_local_id,
			r.client_player_slot);

		// Emit the recent-action history (oldest first).
		fprintf(sink, "  recent_actions     : %zu entries (oldest first)\n",
			r.recent_actions_count);
		if (r.recent_actions_count > 0) {
			// The ring buffer tail points to the next-write slot; walk
			// backwards from tail to recover entries in oldest-first order.
			size_t cap = desync_report::recent_actions_capacity;
			size_t oldest = (r.recent_actions_tail + cap - r.recent_actions_count) % cap;
			for (size_t j = 0; j < r.recent_actions_count; ++j) {
				size_t idx = (oldest + j) % cap;
				const recent_action_entry& a = r.recent_actions[idx];
				fprintf(sink, "    [%zu] frame=%d owner=%d action=0x%02x\n",
					j, a.frame, a.owner, (unsigned)a.action_id);
			}
		}
	}
	fflush(sink);
	return (int)count;
}

} // namespace bwgame

#endif // BWGAME_SYNC_PROTOCOL_H
