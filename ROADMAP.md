# Roadmap — OpenSnowstorm

OpenSnowstorm is building toward being the **OpenMW of StarCraft: Brood War**: a fully open-source, community-maintained engine that runs Brood War content with complete fidelity — and eventually surpasses the original in moddability, platform support, and long-term preservation.

Like OpenMW, we get there in layers: first a deterministic, testable engine core; then a complete client experience; then enhancements the original never had.

For deeper technical breakdowns, see:

- `docs/modernization-roadmap.md` (detailed phased plan)
- `docs/architecture.md` (module map / ownership model)
- `docs/phase-1-kickoff.md` (active decomposition notes)
- `docs/phase-2-kickoff.md` (active sync/replay reliability slice)
- `docs/phase-3-kickoff.md` (platform/CI hardening kickoff)
- `docs/phase-4-kickoff.md` (complete client experience foundations kickoff)
- `docs/broodwar-compatibility.md` (compatibility matrix + validation backlog)

---

## North star (the OpenMW parallel)

OpenMW proved that a community of volunteers can:

1. Understand and replicate a commercial game engine at bit-perfect fidelity.
2. Build cleaner, more maintainable code than the original.
3. Add capabilities the original never had (64-bit, Lua scripting, GPU rendering, etc.).
4. Ensure the game can be preserved and played decades into the future.

**That is exactly what OpenSnowstorm is doing for StarCraft: Brood War.**

Concretely, "success" means:

- A player can install Brood War data files, point OpenSnowstorm at them, and play a complete game — single-player campaigns, multiplayer, custom maps — without touching Blizzard's binaries.
- Bot authors can embed the engine headlessly via the BWAPI-compatible API and test against a deterministic, debuggable simulation.
- Modders can add new units, mechanics, and campaigns using documented, stable engine APIs.
- The engine runs on Linux, Windows, macOS, and the Web (Emscripten) without proprietary dependencies.

---

## Milestones

### Phase 0 — Foundations (Now)

*Goal: make the engine safe to change and easy to contribute to.*

- **Determinism regression harness**
  - Replay-driven "golden" checks (frame hashes + key invariants)
  - CI: run on every PR touching simulation code
- **Build normalization**
  - Single top-level CMake entry (or presets) building engine + UI + shim consistently
  - Documented toolchain and dependency requirements
- **Contributor ergonomics**
  - Formatting/lint guidance (non-disruptive baseline)
  - Quick-start contributor docs, issue labels, good-first-issue list
- **Performance baseline**
  - `--bench` mode for headless simulation throughput measurement (done)
  - `perf_metrics.h` frame-timer infrastructure (done)
  - Pre-allocated unit-finder spatial index (done)

### Phase 1 — Architecture Decomposition

*Goal: reduce the 22k-line monolith blast radius and clarify ownership.*

- **Split `bwgame.h`** into focused modules behind a compatibility facade
  - Continue the existing extraction work (`bwgame_tables.h`, `bwgame_state.h`, `sync_protocol.h`)
  - Eliminate `simulation_constants.h` duplication (done — now delegates to `bwgame_tables.h`)
- **Explicit ownership and lifetimes**
  - Reduce hidden coupling between sync/actions/state layers
  - const-correctness passes on low-risk paths

### Phase 2 — Sync + Replay Reliability

*Goal: make multiplayer and replay infrastructure production-grade.*

- **Protocol boundaries and versioning**
  - Explicit message schema documentation + compatibility strategy
- **Desync diagnostics**
  - Structured desync report artifact: first divergent frame, recent actions/hashes, client ID
- **Replay format durability**
  - Validation tooling, forward-compat metadata, fuzz targets for binary readers

### Phase 3 — Platform & UI Hardening

*Goal: decouple SDL2 from the architecture; support more backends.*

- **UI backend abstraction**
  - Define a narrow platform/window interface so SDL2 is one backend, not the only one
  - Separate input translation, rendering surface management, and platform lifecycle
- **Headless entrypoint hardening**
  - Clean headless mode without SDL linkage (pure simulation binary)
  - Suitable for CI, bot testing, benchmarking
- **Runtime UX improvements**
  - Configurable hotkeys, runtime config file
  - Debug overlays: FPS, frame number, sync state, entity diagnostics

### Phase 4 — Complete Client Experience

*Goal: a player can run Brood War end-to-end with OpenSnowstorm.*

- **Single-player client foundation (landed)**
  - `gfxtest --map <file.scx|file.scm>` can now run an interactive single-player map session.
  - Local unit control path includes selection, default right-click orders, stop/hold, attack-move/patrol target modes, and control groups.
  - Local-vision rendering path (fog/explored filtering for terrain, sprites, and minimap) is now wired for non-observer play.
  - This is a foundation milestone, not full UX parity with Brood War's campaign/lobby frontend.
- **Campaign engine**
  - Trigger system completeness (all StarEdit triggers)
  - Briefing room support
  - Campaign save/load
- **Lobby and matchmaking**
  - LAN lobby (UDP broadcast peer discovery)
  - Integration with open community ladders (e.g., ICCUP-compatible protocols)
- **Sound and music**
  - Complete SFX pipeline (unit voices, ambient, combat)
  - Music playback (SCM-format soundtrack)
- **Full renderer**
  - Hardware-accelerated sprite compositing (OpenGL or Vulkan backend)
  - Widescreen support
  - Optional HD graphics pack support

### Phase 5 — Beyond the Original

*Goal: do what the original engine never could.*

- **Modding API**
  - Lua scripting for triggers, AI, unit behaviours
  - Custom unit type definitions via data file extension
  - Workshop-style mod loading
- **64-bit / extended limits**
  - Unit cap raised above 1700 (opt-in, non-default for compatibility)
  - Map size beyond 256×256
- **Improved AI**
  - Open AI scripting interface for custom computer opponents
  - Integration hooks for ML-based bots (OpenAI gym-style step API)
- **Cross-platform preservation**
  - Emscripten/WASM build for in-browser play (already partially working)
  - Mobile backends

---

## Contribution-sized "good first" work

- **Docs**: tighten build/run instructions, diagram updates, API surface explanations
- **Refactor slices**: extract a focused helper/module behind a compatibility facade
- **Testing infrastructure**: replay fixtures + hash checkpoints + harness wiring
- **Diagnostics**: log categories (`sync`, `replay`, `ui`, `data`, `perf`) and consistent error paths
- **Performance**: profile hot paths with `perf_metrics.h` instrumentation; submit findings as issues

---

## What we won't do (on purpose)

- Bundle Blizzard assets or ship proprietary game content
- Do a "big rewrite" that breaks determinism and compatibility in exchange for aesthetics
- Require proprietary tools or platforms to build and run

---

## Success metrics

- **Determinism**: 100% pass rate on golden-hash replay fixtures across Linux/Windows/WASM
- **Performance**: sim-only throughput ≥ 10× real-time on modern hardware (measure with `--bench`)
- **Compatibility**: all stock single-player campaign maps complete without divergence
- **Build time**: incremental rebuild < 30 seconds for a single-subsystem change
- **Onboarding**: new contributor can build and run a replay check in < 30 minutes
