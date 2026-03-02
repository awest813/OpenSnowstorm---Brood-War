# Phase 1 Kickoff: Architecture Decomposition

This document tracks the first vertical slice for **Phase 1** from `docs/modernization-roadmap.md`.

## Initial slice completed

- Extracted simulation lookup constants from `bwgame.h` into `simulation_constants.h`.
- Kept `bwgame.h` as a compatibility facade by including the new header.

## Why this is phase-1 aligned

- Starts decomposition of the monolithic `bwgame.h` surface area (Phase 1.1).
- Isolates large constant tables into a dedicated module boundary (Phase 1.3).
- Minimizes downstream breakage by preserving existing include paths and symbol names.

## Next slices

1. Move simulation state-only structs from `bwgame.h` into a dedicated state header.
2. Split unit/order behavior helpers into targeted headers.
3. Separate combat/math helpers into their own translation units where practical.
