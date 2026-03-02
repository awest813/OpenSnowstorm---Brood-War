# OpenSnowstorm / OpenBW Engine Modernization & QoL Roadmap

This roadmap is based on a quick architecture pass over the current codebase, focusing on maintainability, portability, determinism safety, and developer quality-of-life.

## Current Snapshot (what the codebase is optimized for today)

- **Core engine logic appears to be concentrated in large header-heavy units** (notably `bwgame.h`), with substantial shared state and data tables.
- **Build setup is fragmented across subprojects** (`ui/` and `mini-openbwapi/`) and currently targets **C++14**.
- **UI backend is SDL2-centric and tightly coupled in one implementation file** (`ui/sdl2.cpp`).
- **Networking/sync logic is integrated with engine/runtime flow** (`sync.h`), including message protocol IDs and scheduling behavior.
- **There are custom container and utility abstractions** (`containers.h`, `static_vector.h`, `circular_vector.h`, etc.) that likely serve determinism/performance goals but increase onboarding complexity.

---

## Modernization Objectives

1. Improve long-term maintainability without breaking deterministic simulation behavior.
2. Reduce integration friction for contributors (build, tests, diagnostics, tooling).
3. Make engine internals easier to reason about by introducing clearer module boundaries.
4. Add pragmatic quality-of-life fixes for everyday development and debugging.

---

## Guiding Principles

- **Determinism first**: every modernization task must define whether/how determinism is preserved.
- **Refactor in vertical slices**: isolate one subsystem at a time (sync, UI, data loading, etc.).
- **Golden replay safety net**: use replay-based regression checks before and after changes.
- **No “big rewrite”**: prioritize incremental compatibility layers and de-risked migration.

---

## Phase 0 (0–2 weeks): Baseline, Safety Nets, and Tooling

### 0.1 Establish deterministic regression harness
- Create a replay-driven test executable/script that:
  - loads a fixed set of representative replays/maps,
  - simulates N frames,
  - records frame hashes / key metrics,
  - diffs against golden outputs.
- Store outputs in versioned fixtures and run in CI.

### 0.2 Build and CI normalization
- Add a top-level CMake entrypoint that drives engine + UI + mini-openbwapi builds in a consistent way.
- Add CI matrix for:
  - Linux (gcc + clang),
  - optional Windows build (if supported by current abstractions),
  - Debug + Release variants.

### 0.3 Developer ergonomics
- Add documented presets (`CMakePresets.json`) for common tasks.
- Add `clang-format` and optionally `clang-tidy` with a minimal initial rule set.
- Add a `CONTRIBUTING.md` with build/test/debug quick-start.

**Deliverables**
- Green CI baseline.
- Determinism check command runnable locally and in CI.
- One-page contributor setup doc.

---

## Phase 1 (2–6 weeks): Architecture Decomposition

### 1.1 Split simulation domains from monolithic headers
- Gradually carve `bwgame.h` into focused modules:
  - simulation state definitions,
  - unit/order systems,
  - combat/math helpers,
  - map/region helpers.
- Keep a compatibility facade to avoid immediate downstream breakage.

### 1.2 Explicit ownership and lifetimes
- Replace ambiguous shared mutable state flows with clear ownership boundaries:
  - engine context,
  - immutable game data assets,
  - per-match mutable state.
- Add const-correctness passes where low risk.

### 1.3 Data table and constant isolation
- Move giant lookup tables into dedicated translation units or generated assets where feasible.
- Annotate table provenance and expected invariants.

**Deliverables**
- Reduced compile blast radius.
- Improved readability and traceability for core systems.

---

## Phase 2 (4–8 weeks): Sync/Replay Reliability and Diagnostics

### 2.1 Formalize sync protocol boundaries
- Encapsulate message IDs and wire encoding/decoding into a dedicated protocol module.
- Add schema-like documentation for each message and versioning strategy.

### 2.2 Observability for desyncs
- Add structured logging around sync frame transitions, action scheduling, and hash checkpoints.
- Add a “desync report” artifact containing:
  - first divergent frame,
  - participating clients,
  - recent action/message history.

### 2.3 Replay format durability
- Introduce replay format version metadata and compatibility checks.
- Add replay validation tooling (header sanity checks, frame stream consistency).

**Deliverables**
- Faster desync triage.
- Better confidence in protocol changes.

---

## Phase 3 (6–10 weeks): Rendering/UI and Platform QoL

### 3.1 UI backend abstraction cleanup
- Define a narrow platform/window interface so SDL2 remains one backend, not the only backend.
- Separate input translation, rendering surface management, and platform lifecycle logic.

### 3.2 Runtime UX improvements
- Add configurable hotkeys and basic runtime config file support.
- Add quality-of-life debug overlays (FPS, frame number, sync state, selected entity diagnostics).
- Add optional headless mode entrypoint for simulation-only runs.

### 3.3 Asset and initialization error handling
- Standardize error reporting paths (human-readable + machine-readable logs).
- Improve startup diagnostics for missing SDL/assets/data files.

**Deliverables**
- Cleaner platform layer.
- Better local debugging and bot/simulation workflows.

---

## Phase 4 (8–12 weeks): Data and API Surface Modernization

### 4.1 Strong typing and API tightening
- Replace magic integers/enums crossing subsystem boundaries with scoped enum wrappers and typed IDs.
- Add API boundaries for “public embedders” vs internal engine-only APIs.

### 4.2 Serialization and loading hardening
- Add bounds/consistency checks in data loaders.
- Add fuzz targets or property-based tests for binary readers where practical.

### 4.3 Dependency strategy refresh
- Evaluate vendored dependencies policy (e.g., ASIO) and define update cadence.
- Document minimum compiler/toolchain versions.

**Deliverables**
- Safer APIs and data handling.
- Easier external integration and upgrades.

---

## QoL Fix Backlog (can run in parallel)

- Add consistent logging macros with categories (`sync`, `replay`, `ui`, `data`, `perf`).
- Add crash-safe assert/report utilities (frame, map, player context in logs).
- Add compile-time feature flags summary printed at startup.
- Add deterministic random seed controls and command-line override.
- Add scriptable benchmark mode for micro/macro performance regressions.
- Add “first-good issue” documentation with architecture map diagrams.

---

## Suggested Priority Order (if resources are constrained)

1. **Determinism regression harness + CI baseline** (highest risk reducer).
2. **Sync diagnostics and replay hardening** (production/debug impact).
3. **Header decomposition and module boundaries** (maintainability multiplier).
4. **UI/platform abstraction and runtime QoL** (developer experience).
5. **Typed API/data hardening and dependency refresh** (long-term reliability).

---

## Success Metrics

- Determinism regression suite pass rate (target: 100% on protected fixtures).
- Median time-to-diagnose desync issue (target: significant reduction).
- Full clean build time and incremental rebuild time (target: measurable reduction).
- New contributor setup time (target: <30 minutes documented path).
- Crash/debug report quality (target: actionable first-pass logs for most failures).

---

## Risks and Mitigations

- **Risk:** determinism regressions during refactors.
  - **Mitigation:** enforce replay/hash checks for every PR touching simulation.
- **Risk:** broad header extraction causes hidden dependency breakage.
  - **Mitigation:** introduce facade headers and migrate call sites incrementally.
- **Risk:** modernization stalls due to unclear ownership.
  - **Mitigation:** assign subsystem owners and phase-specific acceptance criteria.

---

## First 5 Concrete Tickets to Start Immediately

1. Add top-level CMake + CI workflow with Debug/Release Linux builds.
2. Implement `determinism_check` tool with golden frame-hash fixtures.
3. Add sync trace logging around scheduled action execution and frame advancement.
4. Extract protocol message enums/serialization from `sync.h` into `sync_protocol.*`.
5. Draft module map document (`docs/architecture.md`) showing current subsystem boundaries.

