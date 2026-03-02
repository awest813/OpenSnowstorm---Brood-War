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

1. Commit a small test replay to `maps/test.rep` and enable the CI `validate-replay` gate.
2. Add `maps/test.hashes` produced by `--record-hashes` and run `--verify-hashes` in CI.
3. Extend `desync_report` with a recent action-history ring buffer for deeper triage.
4. Begin combat fixture scenarios for damage-type / armor / splash edge cases.
5. Cover remaining BW action IDs (59, 63–69, 72–86) with proper payload-size skip entries.

## Definition of done for each compatibility slice

Each slice should include:

1. A focused scenario (fixture or scripted action sequence).
2. A deterministic pass/fail assertion (hash/invariant/expected state).
3. A brief note in this tracker linking to implementation and verification command(s).

This keeps compatibility work observable and prevents silent regressions while the codebase is modernized.
