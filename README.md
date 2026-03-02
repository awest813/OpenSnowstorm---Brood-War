# OpenSnowstorm — The OpenMW of StarCraft: Brood War

OpenSnowstorm is an **open-source, community-driven engine reimplementation** of StarCraft: Brood War — aiming to be for StarCraft what [OpenMW](https://openmw.org) is for Morrowind: a clean-room, modern, free-software engine that can run the original game's content with full fidelity, and ultimately exceed the original in moddability, platform support, and long-term preservation.

> **Current state:** The engine simulation, replay, sync, and BWAPI-compatible integration layers are functional.  
> A first interactive **single-player map client foundation** now exists in `gfxtest` (`--map ...`).  
> A complete client experience (campaign frontend, lobby, matchmaking, full UX parity) remains on the long-term roadmap.  
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
| Replay validation sanity check | Added (see `--validate-replay`) |
| Replay hash fixtures (record/verify) | Added (see `--record-hashes`, `--verify-hashes`) |
| Single-player map mode (`gfxtest --map`) | Added (foundation slice) |
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
  - Supports `--map <file.scx|file.scm>` for interactive single-player map runs
  - Supports `--bench <frames>` for headless simulation benchmarking
  - Supports `--headless` for simulation without rendering
  - Supports `--validate-replay` for replay header/frame-stream validation
  - Supports `--record-hashes <fixture.txt>` to generate frame-hash checkpoints
  - Supports `--verify-hashes <fixture.txt>` to assert deterministic checkpoints
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

A top-level CMake build is available:

```bash
cmake -B build -DOPENSNOWSTORM_BUILD_UI=ON -DOPENSNOWSTORM_BUILD_BWAPI=ON
cmake --build build -j
```

Subproject builds are still supported:

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

### Single-player map mode (foundation)

```bash
# Interactive single-player game on a map (requires your own game data files + map file)
./build/gfxtest --map maps/YourMap.scx --game-type melee --local-race terran --enemy-race zerg
```

Optional map-mode arguments:

- `--local-player <0-7>`
- `--enemy-player <0-7>`
- `--game-type <melee|ums>`
- `--local-race <zerg|terran|protoss|random>`
- `--enemy-race <zerg|terran|protoss|random>`

Controls in map mode:

- **Left click/drag**: select units
- **Right click**: issue default order
- **Middle mouse drag**: move camera
- **S**: stop
- **H**: hold position
- **A**: attack-move (applies to next right click)
- **T**: patrol (applies to next right click)
- **1–0**: recall control group
- **Ctrl + 1–0**: set control group
- **Shift + 1–0**: add to control group
- **Space / P**: pause
- **U**: speed up simulation
- **Z / D**: slow down simulation

### Benchmark (no game data required for the build itself)

```bash
# Run 5000 simulation frames as fast as possible; reports fps/latency stats
./gfxtest --bench 5000 --replay maps/myrep.rep
```

### Replay validation sanity check

```bash
# Validate replay container/header + action frame-stream consistency
./gfxtest --validate-replay --replay maps/myrep.rep
```

Expected result:
- `validate: PASS` with replay summary details on success (exit code `0`)
- `validate: FAIL (...)` on malformed/incompatible replay data (non-zero exit code)

### Replay hash fixture record + verify

```bash
# Record checkpoints every 240 frames into a fixture file
./gfxtest --record-hashes maps/test.hashes --hash-interval 240 --replay maps/myrep.rep

# Verify replay state hashes against the fixture file
./gfxtest --verify-hashes maps/test.hashes --replay maps/myrep.rep
```

Fixture format (`maps/test.hashes`):

```text
# OpenSnowstorm replay hash fixture v1
replay: maps/myrep.rep
# frame hash
0 0x12345678
240 0x90abcdef
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
