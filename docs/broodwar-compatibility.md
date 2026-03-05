# Brood War Compatibility Tracker

This document is the working tracker for OpenSnowstorm's **behavioral compatibility** with StarCraft: Brood War.

The goal is to make compatibility work concrete, testable, and incrementally shippable instead of treating it as a vague long-term promise.

## Status labels

- **Unmapped**: not yet decomposed into concrete checks.
- **Planned**: scoped with at least one measurable check to implement.
- **In progress**: active engineering work is happening.
- **Partially validated**: at least one check exists, but coverage is incomplete.
- **Validated**: covered by repeatable checks with stable expectations.

## Compatibility matrix

| Area | Why it matters | Status | Notes / next step |
|---|---|---|---|
| Core simulation determinism | Replay and sync correctness depend on frame-identical outcomes. | **Partially validated** | Insync hash now includes frame counter, unit owner, and order type for finer divergence detection. `gfxtest --record-hashes` and `--verify-hashes` provide replay fixture checkpoints; next step is enforcing them in CI. |
| Order/AI behavior parity | Unit command edge-cases drive real gameplay differences. | **In progress** | Patrol (action 29) and building Land (action 36) are now dispatched. Continue with remaining BW-specific actions. |
| Combat and damage interactions | Small damage/timing mismatches cascade into macro-level divergence. | **Planned** | Start fixture scenarios for cooldown timing, armor/upgrade interactions, splash behavior. |
| Economy timings | Worker and production timing parity is essential for bots and replays. | **Partially validated** | Starting gas is now loaded from replays and applied via `setup_info.starting_gas`. Resource-type-1 custom starts correctly set both minerals and gas. |
| Replay format compatibility | Durable replay I/O is required for regression and tooling interoperability. | **Partially validated** | `is_broodwar` flag now captured in `replay_state`. Starting gas round-trips correctly. Replay saver writes `starting_gas` from setup info. `gfxtest --validate-replay` now validates replay container/header and action frame-stream consistency. |
| Multiplayer sync behavior | Action scheduling and frame stepping must stay BWAPI-compatible. | **Partially validated** | Protocol version constant (`sync_protocol_version`) and `desync_report` structure added. Hash divergences now emit a structured diagnostic before client kill. |

## Current phase focus

- **Phase 1** established protocol/constants decomposition foundations (`sync_protocol.h`, `simulation_constants.h`).
- **Phase 2 kickoff** is tracked in `phase-2-kickoff.md` with concrete sync/replay reliability slices and exit criteria.
- **Phase 3 kickoff** is tracked in `phase-3-kickoff.md` covering CI infrastructure, BW action completions, and desync diagnostics.
- **Phase 4 kickoff** is tracked in `phase-4-kickoff.md` covering complete-client foundations, campaign-readiness tracking, and user-facing regression gates.

### Changes landed (Phase 2)

| Change | Files | What it enables |
|---|---|---|
| `is_broodwar` on `tech_type_t` | `data_types.h`, `data_loading.h` | Tech types now carry a BW flag (derived from ID >= 24), matching units/upgrades. Enables BW-aware tech filtering. |
| Starting gas in replay round-trip | `bwgame.h`, `replay.h`, `replay_saver.h` | `setup_info_t.starting_gas` added. Replay reader feeds gas into map load. Replay saver writes the stored value instead of zero. |
| Patrol action (29) | `actions.h` | BW replays containing patrol commands no longer cause unknown-action errors. Group-move logic applied. |
| Building Land action (36) | `actions.h` | Flying buildings can now be landed via the replay/sync action path, matching BW's action 36 wire format. |
| Protocol version constant | `sync_protocol.h` | `sync_protocol_version` (currently 1) provides a handshake-time compatibility check point for future protocol changes. |
| `desync_report` struct | `sync_protocol.h` | Structured desync diagnostic: frame, hash index, expected/received hash, client ID/slot. |
| Desync report emission | `sync.h` | On insync mismatch, a `desync_report` is appended to `sync_state::desync_reports` and `desync_detected` is set before the client is killed. |
| Richer insync hash | `sync.h` | Hash now includes `current_frame`, unit `owner`, and `order_type->id` for each visible unit, improving divergence granularity. |
| Replay validation command | `ui/gfxtest.cpp` | `--validate-replay` performs replay load/header checks plus action frame-stream consistency validation and returns non-zero on failure. |

### Changes landed (Phase 3 kickoff)

| Change | Files | What it enables |
|---|---|---|
| Top-level `CMakeLists.txt` | `CMakeLists.txt` | Single-command configure+build for `gfxtest` and `mini-openbwapi` from the repository root. |
| `CMakePresets.json` | `CMakePresets.json` | Named presets: Debug/Release × GCC/Clang and a headless `no-ui` preset. |
| GitHub Actions CI | `.github/workflows/ci.yml` | Automated Linux build matrix (gcc + clang, Debug + Release); `validate-replay` gate template included. |
| `sync_protocol_min_peer_version` | `sync_protocol.h` | Explicit minimum peer version for handshake rejection; inline compatibility policy documentation. |
| `write_desync_reports()` | `sync_protocol.h` | Structured `key: value` desync output to any `FILE*` sink (stderr by default) for CI consumption. |
| Automatic stderr desync emission | `sync.h` | `write_desync_reports(stderr, …)` called on every insync mismatch before killing the diverging client. |
| Graceful skip for unknown actions | `actions.h` | Unknown action IDs log a warning and consume remaining frame-chunk bytes instead of crashing. |
| `read_action_skip<N>` template | `actions.h` | Zero-simulation skip for observer-only actions with known payload sizes. |
| Actions 55–58, 60–62, 70–71, 89, 91 | `actions.h` | Save/load game, restart, game-speed changes, pause/resume, vision toggle, allied-victory toggle, and BW replay markers are now consumed cleanly. |
| Replay hash fixture tooling | `ui/gfxtest.cpp` | `--record-hashes` emits deterministic frame-hash fixtures at configurable intervals; `--verify-hashes` asserts replay checkpoints against those fixtures. |

### Changes landed (Phase 4 kickoff)

| Change | Files | What it enables |
|---|---|---|
| Replay seek-state bootstrap hardening | `ui/gfxtest.cpp` | Replay frame seeking now guarantees at least one snapshot before lookup, avoiding an empty-state dereference during early seek/rewind transitions. |
| UMS slot-topology-preserving map startup | `ui/gfxtest.cpp` | `gfxtest --map --game-type ums` now preserves authored participant layout instead of forcing a strict two-slot open/computer conversion path; local and optional enemy slots are selected with mode-aware validation. |
| Expanded local-command parity surface | `ui/ui.h`, `actions.h`, `ui/gfxtest.cpp`, `README.md` | Single-player map mode now exposes additional tactical actions (cancel, burrow toggle, siege toggle, cloak toggle, return cargo, unload all) via hotkeys and command panel entries; action success propagation for cloak/unload paths is now reported correctly to UI callers. |
| Multi-selection tactical command panel parity | `ui/ui.h`, `ui/gfxtest.cpp`, `README.md` | When multiple local units are selected in live map mode, the command panel now exposes stop/hold plus attack-move/patrol mode selectors, matching hotkey functionality without requiring a single-unit selection. |
| Lift-off/land and cancel-nuke map-mode command parity | `ui/ui.h`, `ui/gfxtest.cpp`, `README.md` | Live map mode now exposes Terran building lift off / land via command panel + context-sensitive `l` hotkey (falling back from unload-all), with landing using armed placement UX. The cancel command path now also surfaces Ghost nuclear launch cancel when available. |
| Full BW action ID coverage (55–91) | `actions.h` | All remaining BW replay/lobby action IDs (59, 63–69, 72–86) now have explicit skip entries; completes non-simulation action coverage from 55–91 and eliminates stderr warning spam for those IDs in replays. |
| Stim pack in single-player command panel | `ui/ui.h` | Marine/Firebat stim pack is now accessible via the command panel and `i` hotkey in live map mode; `live_command_can_stim` gates readiness on tech unlock state. |
| Archon / Dark Archon merge in command panel | `ui/ui.h` | High Templar / Dark Templar now show a Merge Archon / Merge Dark Archon ability in the command panel and via the `m` hotkey, gated by the relevant tech. |
| Tab: center camera on selection | `ui/ui.h`, `ui/gfxtest.cpp` | Pressing Tab centers the camera on the centroid of the current unit selection in live map mode; aids navigation on large maps. |
| Targeted spell ability surface (25 spells) | `ui/ui.h` | Science Vessel (scanner sweep, defensive matrix, irradiate, EMP), Battlecruiser (yamato gun), Ghost (lockdown), Vulture (spider mines), Medic (heal, restoration, optical flare), Queen (spawn broodlings, parasite, ensnare, infestation), Defiler (dark swarm, plague, consume), High Templar (psionic storm, hallucination), Arbiter (recall, stasis field), Dark Archon (mind control, feedback, maelstrom), Corsair (disruption web) — all now appear in the command panel and arm a targeting mode; right-click fires the spell. |
| `--headless-map` smoke-test mode | `ui/gfxtest.cpp` | `gfxtest --map <file> --headless-map [<frame-limit>]` runs a map session headlessly up to `frame_limit` frames (default 72000) and exits cleanly, enabling CI smoke tests for map-load + simulation paths without a display. |
| `--gen-test-replay` fixture generator | `ui/gfxtest.cpp`, `replay_saver.h` | `gfxtest --gen-test-replay <out.rep> --map <file> [--frames <n>] [--record-hashes <out.hashes>]` generates a self-contained deterministic replay fixture from any BW map file; enables creation of `maps/test.rep` + `maps/test.hashes` without requiring a live game session. |
| Debug overlay with F3 toggle | `ui/ui.h`, `ui/gfxtest.cpp` | Pressing F3 (or passing `--debug-overlay`) enables a top-left HUD showing current frame number, draw FPS, and game speed multiplier with a paused indicator; works in both replay and live-map modes. |
| `--debug-overlay` CLI flag | `ui/gfxtest.cpp` | `gfxtest --debug-overlay` starts with the debug HUD visible, enabling diagnostic capture without interactive input. |
| Desync action-history ring buffer | `sync_protocol.h`, `sync.h` | `desync_report` now carries a 16-entry ring buffer of the most-recently executed action IDs (frame, owner, action_id) snapshotted at mismatch time; `write_desync_reports` prints them oldest-first for triage; `sync_functions` overrides `on_action` to maintain the buffer in `sync_state`. |
| Quicksave / quickload (F5 / F8) | `ui/ui.h`, `ui/gfxtest.cpp` | Single-player live map mode now supports an in-memory quicksave slot. F5 deep-copies `state`, `action_state`, and APM counters; F8 restores from the slot and resets the victory/defeat latch. The game auto-pauses on mission victory or defeat. |

### Changes landed (Phase 4 continuation – trigger expansion)

| Change | Files | What it enables |
|---|---|---|
| `switches[256]` state field | `bwgame_state.h` | 256-element global switch array; trigger actions Set Switch (40) and condition Switch (11) now operate on persistent simulation state. |
| `countdown_timer` state field | `bwgame_state.h` | Global countdown timer in game seconds; decremented ~once per second in `process_triggers`; enables timer-based trigger conditions and Set Countdown Timer actions. |
| `unit_deaths[12][228]` state field | `bwgame_state.h` | Per-player per-unit-type death counters; incremented in `destroy_unit_impl`; enables Deaths (15) and Kill (5) trigger conditions. |
| Death tracking in `destroy_unit_impl` | `bwgame.h` | Increments `st.unit_deaths[owner][uid]` when a non-turret, non-refinery unit is fully destroyed; feeds the Deaths/Kill conditions. |
| New trigger conditions (1,4,5,11,13,15,21,23) | `bwgame.h` | Countdown Timer, Accumulate resources, Kill/Deaths, Switch state, Never, Score conditions; unknown types now return false gracefully. |
| New trigger actions (5–13,16,17,21,23,27–33,36,40,47–52,55–59,61–64) | `bwgame.h` | Pause/Unpause Game, Transmission, Play Sound, Display Text Message, Center View, Create Unit with Properties, Set Mission Objectives, Move Unit to Location, Set Alliance Status, Set Score, Set Countdown Timer, Kill Unit at Location, Leaderboard no-ops, Draw, Give Units to Player, Set Switch, Modify Unit HP/Energy/Shields, Set Next Scenario; unknown types silently skipped. |
| Set Resources subtract bug fix | `bwgame.h` | Trigger action 26 subtract path was unreachable (`num_n==8` matched add first); corrected to `num_n==9`. |
| `trigger_unit_matches_filter()` helper | `bwgame.h` | Extracted shared unit-type predicate from duplicated inline code in kill/remove/order/give actions; reduces future maintenance surface. |
| `get_map_string()` in `state_functions` | `bwgame.h` | Mission text accessible from all trigger action handlers without going through `game_load_functions`. |
| Trigger event virtual callbacks | `bwgame.h` | `on_trigger_display_text`, `on_trigger_transmission`, `on_trigger_center_view`, `on_trigger_set_objectives`, `on_trigger_set_next_scenario` hooks in `state_functions`; UI layer can override to surface mission text and camera cues. |

## Campaign-readiness tracker (Phase 4 foundation)

| Area | Priority | Status | Current blocker | Deterministic check |
|---|---|---|---|---|
| Trigger behavior parity (mission progression) | P0 | **In progress** | Core trigger ops now covered; remaining gaps are leaderboard/score displays, briefing-specific conditions, and campaign transition. | `./gfxtest --validate-replay --replay <campaign-trigger-fixture.rep>` must print `validate: PASS` and exit `0` once fixture is landed. |
| Briefing entry/exit flow stability | P0 | **Planned** | No dedicated fixture yet for briefing-to-mission transition loop and cancel/continue handling. | `./gfxtest --verify-hashes <briefing-flow.hashes> --replay <briefing-flow.rep>` must print `verify-hashes: PASS` and exit `0` once fixture is landed. |
| Save/load state restore invariants | P1 | **Partially validated** | In-memory quicksave (F5) and quickload (F8) are now wired in single-player live map mode; state deep-copies via existing `copy_state` infrastructure. Full file-backed save/load and a replay-backed restore fixture remain for the next slice. | `./gfxtest --verify-hashes <save-load-restore.hashes> --replay <save-load-restore.rep>` must print `verify-hashes: PASS` and exit `0` once fixture is landed. |

### Phase 4 kickoff owners / next slices

| Slice | Owner | Next concrete step |
|---|---|---|
| Trigger coverage audit | Engine gameplay | Enumerate mission-critical trigger opcodes and create one replay fixture per top-priority opcode family. |
| Briefing flow scaffolding | UI/runtime | Add one deterministic fixture that enters briefing and transitions to mission start without soft-lock. |
| Save/load foundation plan | Engine state/replay | Document save-state schema boundary and add a restore hash-check fixture. |

### Validation command (Phase 2 replay sanity check)

- Command: `./gfxtest --validate-replay --replay <path/to/replay.rep>`
- Expected pass signal: output contains `validate: PASS` and process exits `0`.
- Expected fail signal: output contains `validate: FAIL (...)` and process exits non-zero.

### Validation command (Phase 3 determinism checkpoints)

- Record fixture: `./gfxtest --record-hashes <fixture.txt> --hash-interval 240 --replay <path/to/replay.rep>`
- Verify fixture: `./gfxtest --verify-hashes <fixture.txt> --replay <path/to/replay.rep>`
- Expected pass signal: output contains `verify-hashes: PASS` and process exits `0`.
- Expected fail signal: output contains `verify-hashes: FAIL (...)` or mismatch lines and exits non-zero.

## Immediate backlog (starter slice)

1. Add a small set of deterministic replay fixtures that exercise high-risk command and combat cases.
2. Gate fixture checks in CI as non-optional compatibility regressions.
3. Expand each fixture with compact invariant summaries (player resources, unit counts, etc.) alongside hashes.
4. Expand this tracker with links to concrete tests/checks as they land.

## Next steps

1. Generate `maps/test.rep` + `maps/test.hashes` from a BW map file using `--gen-test-replay` and commit them to enable the CI `validate-replay` gate.
2. Add `maps/test.hashes` produced by `--record-hashes` and run `--verify-hashes` in CI.
3. Extend `desync_report` with a recent action-history ring buffer for deeper triage.
4. Begin combat fixture scenarios for damage-type / armor / splash edge cases.
5. Add briefing-flow fixture (briefing entry/exit loop without soft-lock).

## Definition of done for each compatibility slice

Each slice should include:

1. A focused scenario (fixture or scripted action sequence).
2. A deterministic pass/fail assertion (hash/invariant/expected state).
3. A brief note in this tracker linking to implementation and verification command(s).

This keeps compatibility work observable and prevents silent regressions while the codebase is modernized.
