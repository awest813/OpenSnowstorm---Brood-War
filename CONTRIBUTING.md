# Contributing to OpenSnowstorm

Thanks for contributing. This project is an engine-focused effort: **determinism and correctness** matter more than “cleanups” that change behavior.

## Ground rules

- **Determinism first**: changes to simulation must be deterministic across supported toolchains/platforms.
- **Small, reviewable PRs**: prefer vertical slices over broad refactors.
- **Compatibility facades are OK**: keep downstream include paths stable while decomposing modules.
- **No proprietary assets**: do not submit Blizzard data files, MPQ contents, or other copyrighted game assets.

## Build prerequisites

- **CMake**: 3.1+
- **Compiler**: a C++14-capable toolchain
- **SDL2**: optional (only needed for UI-enabled builds)

## Build (current structure)

There is not yet a single top-level CMake project; build subprojects directly.

BWAPI shim library (no UI):

```bash
cmake -S mini-openbwapi -B build/mini-openbwapi -DOPENBW_ENABLE_UI=OFF
cmake --build build/mini-openbwapi -j
```

BWAPI shim with UI enabled:

```bash
cmake -S mini-openbwapi -B build/mini-openbwapi -DOPENBW_ENABLE_UI=ON
cmake --build build/mini-openbwapi -j
```

UI library only:

```bash
cmake -S ui -B build/ui
cmake --build build/ui -j
```

## Making changes safely

### Simulation changes

- Prefer adding/refactoring code **behind existing interfaces**.
- If you extract a module, keep `bwgame.h` as a compatibility facade.
- If you touch sync/replay, keep wire-format stability in mind (`sync_protocol.h` IDs should remain append-only).

### Logging and diagnostics

- Prefer structured logs that can be turned on/off by category.
- Errors should include actionable context (frame number, player id, map name when available).

## Style

This repo is currently in a transitional state. Keep changes consistent with local style in the touched files.

If you introduce formatting tooling (e.g., clang-format), keep the first step minimal and non-disruptive.

## Submitting a PR

- Describe **what changed**, **why**, and **how determinism was preserved**.
- Link to any relevant issue/roadmap milestone.
- Keep PRs narrow: one subsystem or one slice at a time.

## Legal / licensing

By contributing, you agree that your contributions will be licensed under this repository’s license (see `LICENSE`).

