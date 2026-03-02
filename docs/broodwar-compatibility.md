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
| Core simulation determinism | Replay and sync correctness depend on frame-identical outcomes. | **In progress** | Expand replay-based hash regression checks and make them part of regular CI runs. |
| Order/AI behavior parity | Unit command edge-cases drive real gameplay differences. | **Planned** | Define high-value order scenarios (retargeting, spell interruption, queueing). |
| Combat and damage interactions | Small damage/timing mismatches cascade into macro-level divergence. | **Planned** | Start fixture scenarios for cooldown timing, armor/upgrade interactions, splash behavior. |
| Economy timings | Worker and production timing parity is essential for bots and replays. | **Planned** | Add checks for mining cadence, larva/morph timing, and addon/build queue transitions. |
| Replay format compatibility | Durable replay I/O is required for regression and tooling interoperability. | **In progress** | Continue format validation + metadata/versioning work tied to replay reader/writer updates. |
| Multiplayer sync behavior | Action scheduling and frame stepping must stay BWAPI-compatible. | **Partially validated** | Add focused tests around latency-buffer and first-frame behavior invariants. |

## Current phase focus

- **Phase 1** established protocol/constants decomposition foundations (`sync_protocol.h`, `simulation_constants.h`).
- **Phase 2 kickoff** is now tracked in `phase-2-kickoff.md` with concrete sync/replay reliability slices and exit criteria.

## Immediate backlog (starter slice)

1. Add a small set of deterministic replay fixtures that exercise high-risk command and combat cases.
2. Record frame hash checkpoints and a compact invariant report for each fixture.
3. Gate fixture checks in CI as non-optional compatibility regressions.
4. Expand this tracker with links to concrete tests/checks as they land.

## Definition of done for each compatibility slice

Each slice should include:

1. A focused scenario (fixture or scripted action sequence).
2. A deterministic pass/fail assertion (hash/invariant/expected state).
3. A brief note in this tracker linking to implementation and verification command(s).

This keeps compatibility work observable and prevents silent regressions while the codebase is modernized.
