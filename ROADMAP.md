# Roadmap (OpenSnowstorm)

This is the **high-level, contributor-facing roadmap** for OpenSnowstorm: an open-source, deterministic engine project targeting **high-fidelity StarCraft: Brood War simulation compatibility**.

For deeper technical breakdowns, see:

- `docs/modernization-roadmap.md` (detailed phased plan)
- `docs/architecture.md` (module map / ownership model)
- `docs/phase-1-kickoff.md` (active decomposition notes)
- `docs/broodwar-compatibility.md` (compatibility matrix + validation backlog)

---

## North star

- **Deterministic, replayable simulation core** with strong regression safeguards
- **Embeddable integration surfaces** (BWAPI-style wrapper, headless stepping)
- **Debuggable sync/replay pipeline** (desync reports, structured traces)
- **Clear module boundaries** (incremental decomposition without breaking consumers)

---

## Milestones

### Now (Phase 0 / foundations)

- **Determinism regression harness**
  - Replay-driven “golden” checks (frame hashes + key invariants)
  - Runs locally and in CI
- **Build normalization**
  - A single top-level build entry (or presets) that can build engine + UI + shim consistently
  - Documented toolchain + dependency requirements
- **Contributor ergonomics**
  - Formatting/lint guidance (minimal, non-disruptive baseline)
  - Quick-start contributor docs and issue labels

### Next (Phase 1 / decomposition)

- **Split `bwgame.h` into smaller modules** while preserving the compatibility facade
  - Continue the existing extraction work (`bwgame_tables.h`, `bwgame_state.h`, `sync_protocol.h`, `simulation_constants.h`)
- **Make ownership and lifetimes explicit**
  - Reduce hidden coupling between sync/actions/state layers

### After (Phase 2 / sync + replay reliability)

- **Protocol boundaries and versioning**
  - Explicit message schema documentation and compatibility strategy
- **Desync diagnostics**
  - Structured “desync report” artifact (first divergent frame, recent actions/messages, hashes)
- **Replay format durability**
  - Validation tooling and forward-compat metadata

### Later (Phase 3+ / platform and API hardening)

- **Platform/UI abstraction cleanup**
  - Keep SDL2 as a backend, but avoid SDL2 being the architecture
  - Headless entrypoint for CI/benchmarks/tools
- **Safer API surfaces**
  - Typed IDs, scoped enums where beneficial, clearer public-vs-internal boundaries

---

## Contribution-sized “good first” work

- **Docs**: tighten build/run instructions, diagram updates, API surface explanations
- **Refactor slices**: extract a focused helper/module behind a compatibility facade
- **Testing infrastructure**: replay fixtures + hash checkpoints + harness wiring
- **Diagnostics**: log categories (`sync`, `replay`, `ui`, `data`, `perf`) and consistent error paths

---

## What we won’t do (on purpose)

- Bundle Blizzard assets or ship proprietary game content
- Do a “big rewrite” that breaks determinism and compatibility in exchange for aesthetics

