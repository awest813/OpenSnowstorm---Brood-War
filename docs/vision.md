# Vision: OpenSnowstorm is the OpenMW of StarCraft: Brood War

## The analogy that defines us

[OpenMW](https://openmw.org) is a community-built, free-software reimplementation of Bethesda's Morrowind engine. It loads the original game data, plays the game faithfully, and has grown to surpass the original in performance, moddability, and cross-platform reach — all without touching a line of proprietary code.

**OpenSnowstorm has exactly this ambition for StarCraft: Brood War.**

We are building a clean-room, open-source engine that:

1. Loads your legally-obtained Brood War data files (MPQ archives, DAT files).
2. Runs the game with bit-perfect simulation fidelity — same unit behaviour, same combat math, same timing.
3. Is fully deterministic and testable: replay-driven regression checks catch any drift.
4. Provides stable integration surfaces for bots, tools, and community infrastructure.
5. Will eventually support a complete game experience: campaigns, multiplayer, custom maps.
6. Will ultimately add capabilities the original never had: modding APIs, wide-screen, modern platforms.

---

## Mission

- **Compatibility**: match Brood War's simulation outcomes as closely as practical — unit behaviour, orders, combat timings, edge cases, and quirks that define the competitive game.
- **Determinism**: identical inputs produce identical frame-by-frame results across supported platforms, toolchains, and build configurations.
- **Embeddability**: provide stable integration surfaces — BWAPI-compatible wrapper, headless stepping, replay driving, benchmark mode.
- **Preservation**: ensure StarCraft: Brood War can be played on any platform decades from now, without depending on Blizzard servers or proprietary binaries.
- **Maintainability**: refactor toward clear module boundaries without jeopardising correctness.

---

## Guiding principles

- **Determinism first**: any change that touches simulation must specify and preserve determinism assumptions. A non-deterministic engine is useless for replay validation, competitive play, and bot testing.
- **Golden replays as tests**: replay-driven regression checks are the primary safety net for all refactors. No simulation change merges without a passing replay check.
- **Incremental decomposition**: keep compatibility facades while splitting large headers/subsystems into smaller modules. Never force downstream code to change unnecessarily.
- **No asset redistribution**: the project does not ship Blizzard assets; users supply their own data. This is non-negotiable.
- **Performance matters**: the engine should run the simulation significantly faster than real-time in headless mode, enabling fast bot training, replay analysis, and CI. Use `perf_metrics.h` instrumentation to measure and track regressions.

---

## What "success" looks like — near term

- A contributor can build and run a deterministic replay/simulation check in under 30 minutes.
- Refactors are routine because golden replays catch behaviour drift early.
- External tools (bots, analysers, debuggers) can embed the engine through a documented API surface.
- Multiplayer sync is observable and debuggable (action logs, hash checkpoints, desync reports).
- `--bench` mode gives a reproducible fps/latency number for any replay; CI tracks it over time.

## What "success" looks like — long term (the OpenMW parallel)

- A player installs Brood War data files, runs OpenSnowstorm, and plays the full game — campaigns, multiplayer, custom maps — without any Blizzard binary.
- The engine runs on Linux, Windows, macOS, and the browser (Emscripten/WASM).
- Modders extend the game with new units, triggers, and campaigns using documented, stable APIs.
- The competitive community can rely on OpenSnowstorm as the reference implementation for replay verification and bot tournaments.
- Brood War is playable 50 years from now.

---

## Non-goals (for now)

- Shipping a commercial-grade launcher, matchmaking service, or content distribution pipeline. (Planned — but not the current focus.)
- Bundling or distributing StarCraft: Brood War data files. (Never.)
- Chasing "modern RTS design" changes — compatibility is the goal, not redesign. (Changes beyond the original's design belong in opt-in modding APIs, not the core.)

---

## Where to go next

- Roadmap overview: `../ROADMAP.md`
- Detailed phased plan: `modernization-roadmap.md`
- Architecture map / subsystem boundaries: `architecture.md`
- Performance tools: `../perf_metrics.h`

---

## Legal / trademark note

StarCraft and StarCraft: Brood War are trademarks of Blizzard Entertainment, Inc.  
OpenSnowstorm is not affiliated with, endorsed by, or sponsored by Blizzard.  
OpenMW is an independent open-source project; OpenSnowstorm is not affiliated with OpenMW.
