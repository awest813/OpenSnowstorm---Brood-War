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
| Core simulation determinism | Replay and sync correctness depend on frame-identical outcomes. | **In progress** | Insync hash now includes frame counter, unit owner, and order type for finer divergence detection. Expand replay-based hash regression checks in CI. |
| Order/AI behavior parity | Unit command edge-cases drive real gameplay differences. | **In progress** | Patrol (action 29) and building Land (action 36) are now dispatched. Continue with remaining BW-specific actions. |
| Combat and damage interactions | Small damage/timing mismatches cascade into macro-level divergence. | **Planned** | Start fixture scenarios for cooldown timing, armor/upgrade interactions, splash behavior. |
| Economy timings | Worker and production timing parity is essential for bots and replays. | **Partially validated** | Starting gas is now loaded from replays and applied via `setup_info.starting_gas`. Resource-type-1 custom starts correctly set both minerals and gas. |
| Replay format compatibility | Durable replay I/O is required for regression and tooling interoperability. | **Partially validated** | `is_broodwar` flag now captured in `replay_state`. Starting gas round-trips correctly. Replay saver writes `starting_gas` from setup info. |
| Multiplayer sync behavior | Action scheduling and frame stepping must stay BWAPI-compatible. | **Partially validated** | Protocol version constant (`sync_protocol_version`) and `desync_report` structure added. Hash divergences now emit a structured diagnostic before client kill. |

## Current phase focus

- **Phase 1** established protocol/constants decomposition foundations (`sync_protocol.h`, `simulation_constants.h`).
- **Phase 2 kickoff** is now tracked in `phase-2-kickoff.md` with concrete sync/replay reliability slices and exit criteria.
- **Phase 2 systems overhaul** landed the following concrete compatibility improvements:

### Changes landed

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

## Immediate backlog (starter slice)

1. Add a small set of deterministic replay fixtures that exercise high-risk command and combat cases.
2. Record frame hash checkpoints and a compact invariant report for each fixture.
3. Gate fixture checks in CI as non-optional compatibility regressions.
4. Expand this tracker with links to concrete tests/checks as they land.

## Next steps

1. Add remaining missing BW replay actions (game-speed changes, vision toggles) behind graceful skip paths.
2. Implement replay validation pass (header + frame-stream consistency check) as documented in `phase-2-kickoff.md`.
3. Wire `desync_report` into a log-file or stderr formatter for CI consumption.
4. Begin combat fixture scenarios for damage-type / armor / splash edge cases.

## Definition of done for each compatibility slice

Each slice should include:

1. A focused scenario (fixture or scripted action sequence).
2. A deterministic pass/fail assertion (hash/invariant/expected state).
3. A brief note in this tracker linking to implementation and verification command(s).

This keeps compatibility work observable and prevents silent regressions while the codebase is modernized.
