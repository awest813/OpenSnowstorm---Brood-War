# OpenSnowstorm — The OpenMW of StarCraft: Brood War

OpenSnowstorm is an **open-source, community-driven engine reimplementation** of StarCraft: Brood War — aiming to be for StarCraft what [OpenMW](https://openmw.org) is for Morrowind: a clean-room, modern, free-software engine that can run the original game's content with full fidelity, and ultimately exceed the original in moddability, platform support, and long-term preservation.

> **Current state:** The engine simulation, replay, sync, and BWAPI-compatible integration layers are functional.  
> A complete playable client experience (lobby, matchmaking, renderer) is on the long-term roadmap.  
> Contributors at all levels are welcome.

---

## The vision: OpenMW for StarCraft

[OpenMW](https://openmw.org) showed what a community can achieve: take a beloved game, understand its engine deeply, reimplement it faithfully from scratch, and build something that surpasses the original in every technical dimension — all while staying fully compatible with the original's data files.

OpenSnowstorm has the same ambition for **StarCraft: Brood War**:

| OpenMW (Morrowind)         | OpenSnowstorm (Brood War)           |
|----------------------------|-------------------------------------|
| Engine reimplementation    | Engine reimplementation             |
| Loads original `.esm` data | Loads original MPQ / DAT data       |
| Deterministic simulation   | Deterministic, replayable simulation|
| Community moddability      | Open bot API, custom maps, scripting|
| Cross-platform             | Linux, Windows, Web (Emscripten)    |
| Full game experience       | *Target:* full client experience    |

**What this means in practice:**
- You supply your own legally-obtained StarCraft: Brood War data files.
- OpenSnowstorm provides the engine that runs them.
- Long-term: multiplayer, custom campaigns, scripted AI, modern platform support — all without touching Blizzard code.

---

## Project status

| Area | Status |
|---|---|
| Deterministic simulation core | Functional |
| Replay loading & playback | Functional |
| BWAPI-compatible wrapper (`mini-openbwapi`) | Functional |
| Multiplayer sync layer | Functional (latency-buffered) |
| SDL2 UI (replay viewer) | Functional |
| Headless / benchmark mode | Added (see `--bench`, `--headless`) |
| Automated determinism test suite | Planned (Phase 0) |
| Full game client (lobby, matchmaking) | Long-term roadmap |

---

## What this is (and is not)

- **This is**: a clean-room engine reimplementation that behaves like Brood War's engine for simulation, replays, bot tooling, and eventually a complete game client.
- **This is not**: Blizzard code, Blizzard assets, or a distribution of StarCraft content.
- **Does not include**: game data files (MPQ contents, sprites, sounds, etc.). You must supply your own legally-obtained Brood War installation.

---

## Repository layout

- **Engine core**
  - `bwgame.h`: compatibility facade for the simulation engine (~22k lines, being decomposed)
  - `bwgame_state.h`, `bwgame_tables.h`: decomposed state types + static lookup tables
  - `simulation_constants.h`: backward-compatibility shim → delegates to `bwgame_tables.h`
  - `actions.h`: command parsing/dispatch and frame driving
  - `replay.h`, `replay_saver.h`: replay I/O
  - `sync.h`, `sync_protocol.h`: multiplayer sync + protocol IDs
- **Performance**
  - `perf_metrics.h`: lightweight frame timing (`frame_timer`, `scope_timer`, `perf_categories`)
- **UI**
  - `ui/`: SDL2 backend, rendering, input glue (optional)
  - Supports `--bench <frames>` for headless simulation benchmarking
  - Supports `--headless` for simulation without rendering
  - Supports `--replay <file>` to specify the replay to load
- **BWAPI shim**
  - `mini-openbwapi/`: lightweight BWAPI-compatible wrapper for engine embedding
- **Docs**
  - `docs/vision.md`: mission statement and guiding principles
  - `docs/architecture.md`: subsystem map + ownership/lifetime model
  - `docs/modernization-roadmap.md`: phased refactor + QoL plan
  - `ROADMAP.md`: contributor-facing milestone overview

---

## Getting started

### Build (developer)

There is no single top-level CMake project yet; the build is organized around subprojects.

Build the BWAPI shim (headless, no SDL):

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

### Benchmark (no game data required for the build itself)

```bash
# Run 5000 simulation frames as fast as possible; reports fps/latency stats
./gfxtest --bench 5000 --replay maps/myrep.rep
```

### Headless simulation (SDL window hidden, simulation only)

```bash
./gfxtest --headless --replay maps/myrep.rep
```

---

## Documentation index

- **Vision / "what this is trying to be"**: `docs/vision.md`
- **Roadmap (high-level)**: `ROADMAP.md`
- **Architecture map**: `docs/architecture.md`
- **Brood War compatibility tracker**: `docs/broodwar-compatibility.md`
- **Modernization details**: `docs/modernization-roadmap.md`
- **Contributing**: `CONTRIBUTING.md`
- **Code of Conduct**: `CODE_OF_CONDUCT.md`
- **Security**: `SECURITY.md`

---

## Contributing

See `CONTRIBUTING.md` for build prerequisites, PR guidance, and good-first-issue pointers.

All contributions must preserve **deterministic simulation** — identical inputs must produce identical frame-by-frame outputs across supported platforms and toolchains.

---

## License

OpenSnowstorm is licensed under the **GNU LGPL v3.0**. See `LICENSE`.

Third-party notices: `THIRD_PARTY_NOTICES.md`.

---

## Legal / trademark notice

StarCraft and StarCraft: Brood War are trademarks of Blizzard Entertainment, Inc.  
OpenSnowstorm is **not affiliated with, endorsed by, or sponsored by Blizzard**.  
OpenMW is an independent open-source project; OpenSnowstorm is not affiliated with OpenMW.
