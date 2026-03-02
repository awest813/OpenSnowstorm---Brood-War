# OpenSnowstorm (OpenBW engine fork) — StarCraft: Brood War engine research & reimplementation

OpenSnowstorm is an open-source, deterministic RTS engine project aiming for **high-fidelity compatibility with StarCraft: Brood War gameplay and tooling**, with a strong emphasis on:

- **Deterministic simulation** (replayable, hash-checkable frame stepping)
- **BWAPI-style integration** (via the included `mini-openbwapi/` shim)
- **Replay + sync infrastructure** for headless simulation and multiplayer experiments
- **Incremental modernization** of a historically header-heavy codebase (without “big rewrites”)

This repository currently contains the **engine core** (simulation/state), **replay**, **actions**, **sync**, an **optional SDL2 UI layer**, and a small **BWAPI-compatible wrapper library**.

## Project status

- **Playable “full game” experience**: not the goal *of this repo alone*. This is primarily the **engine core** and integration building blocks.
- **Engineering focus**: correctness, determinism safety, refactoring into clearer modules, and making the project easier to build/extend.

If you want a more batteries-included BWAPI setup, see the upstream BWAPI integration work at `https://github.com/OpenBW/bwapi`.

## What this is (and is not)

- **This is**: a clean-room engine implementation intended to behave like Brood War’s engine for simulation, replays, and bot tooling.
- **This is not**: Blizzard code, Blizzard assets, or a distribution of StarCraft / Brood War content.
- **This does not include**: game data files (MPQ contents, sprites, sounds, etc.). You must provide your own legally obtained Brood War data to run UI/data-loading workflows.

## Repository layout (high-level)

- **Engine core**
  - `bwgame.h`: compatibility facade for the simulation engine
  - `bwgame_state.h`, `bwgame_tables.h`: decomposed state + lookup tables
  - `actions.h`: command parsing/dispatch and frame driving
  - `replay.h`, `replay_saver.h`: replay I/O
  - `sync.h`, `sync_protocol.h`: multiplayer sync + protocol IDs
- **UI**
  - `ui/`: SDL2 backend and rendering/input glue (optional)
- **BWAPI shim**
  - `mini-openbwapi/`: lightweight BWAPI-ish wrapper for engine embedding
- **Docs**
  - `docs/architecture.md`: subsystem map + ownership/lifetime model
  - `docs/modernization-roadmap.md`: phased refactor + QoL plan
  - `docs/phase-1-kickoff.md`: current decomposition tracking

## Getting started (developer build)

There is **no single top-level CMake project** yet; the build is organized around subprojects.

Build the BWAPI shim as a library:

```bash
cmake -S mini-openbwapi -B build/mini-openbwapi -DOPENBW_ENABLE_UI=OFF
cmake --build build/mini-openbwapi -j
```

Build with SDL2 UI enabled:

```bash
cmake -S mini-openbwapi -B build/mini-openbwapi -DOPENBW_ENABLE_UI=ON
cmake --build build/mini-openbwapi -j
```

Build the UI library alone:

```bash
cmake -S ui -B build/ui
cmake --build build/ui -j
```

## Documentation index

- **Vision / “what this is trying to be”**: `docs/vision.md`
- **Roadmap (high-level)**: `ROADMAP.md`
- **Architecture map**: `docs/architecture.md`
- **Modernization details**: `docs/modernization-roadmap.md`
- **Contributing**: `CONTRIBUTING.md`
- **Code of Conduct**: `CODE_OF_CONDUCT.md`
- **Security**: `SECURITY.md`

## License

OpenSnowstorm is licensed under the **GNU LGPL v3.0**. See `LICENSE`.

Third-party notices: `THIRD_PARTY_NOTICES.md`.

## Legal / trademark notice

StarCraft and StarCraft: Brood War are trademarks of Blizzard Entertainment, Inc.  
OpenSnowstorm is **not affiliated with, endorsed by, or sponsored by Blizzard**.
