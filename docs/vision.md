# Vision: what OpenSnowstorm is trying to be

OpenSnowstorm is an open-source engine project focused on **faithfully reproducing StarCraft: Brood War gameplay behavior** in a way that is **deterministic, testable, and embeddable**.

This repository is intentionally “engine-first”: it prioritizes the simulation, replay, and sync foundations that enable bots, headless testing, tooling, and (eventually) multiple frontends.

## Mission

- **Compatibility**: match Brood War’s simulation outcomes as closely as practical (unit behavior, orders, timings, quirks).
- **Determinism**: identical inputs produce identical frame-by-frame results across supported platforms/toolchains.
- **Embeddability**: provide stable integration surfaces (BWAPI-style wrapper, headless stepping, replay driving).
- **Maintainability**: refactor toward clear module boundaries without jeopardizing correctness.

## Guiding principles

- **Determinism first**: any change that touches simulation must specify and preserve determinism assumptions.
- **Golden replays as tests**: replay-driven regression checks are the primary safety net for refactors.
- **Incremental decomposition**: keep compatibility facades while splitting large headers/subsystems into smaller modules.
- **No asset redistribution**: the project does not ship Blizzard assets; users supply their own data.

## What “success” looks like

- A contributor can build and run a deterministic replay/simulation check quickly.
- Refactors are routine because golden replays catch behavior drift early.
- External tools (bots, analyzers, debuggers) can embed the engine through a documented API surface.
- Multiplayer sync is observable and debuggable (action logs, hash checkpoints, desync reports).

## Non-goals (for this repo)

- Shipping a complete commercial-grade launcher, matchmaking service, or content distribution pipeline.
- Bundling or distributing StarCraft: Brood War data files.
- Chasing “modern RTS design” changes—compatibility is the goal, not redesign.

## Where to go next

- Roadmap overview: `../ROADMAP.md`
- Detailed phased plan: `modernization-roadmap.md`
- Architecture map / subsystem boundaries: `architecture.md`

## Legal / trademark note

StarCraft and StarCraft: Brood War are trademarks of Blizzard Entertainment, Inc.  
OpenSnowstorm is not affiliated with, endorsed by, or sponsored by Blizzard.

