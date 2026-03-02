# OpenSnowstorm / OpenBW – Architecture Map

This document describes the current module boundaries of the engine after the
Phase 1 decomposition of the original monolithic `bwgame.h`.  It is intended
as an onboarding reference and as the shared source of truth for further
refactoring work in later phases.

---

## High-Level Subsystem Map

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          Consumer / Integration                          │
│        mini-openbwapi/openbwapi.cpp   ui/sdl2.cpp   ui/gfxtest.cpp      │
└────────────────────────────┬─────────────────────────────────────────────┘
                             │ includes
┌────────────────────────────▼─────────────────────────────────────────────┐
│                          sync.h  (network sync layer)                    │
│   sync_state, sync_functions, sync_server_noop                           │
│   • schedules / dispatches client actions across the latency buffer      │
│   • drives next_frame() via action_functions                             │
│   Depends: bwgame.h, actions.h, replay.h, replay_saver.h,               │
│            sync_protocol.h                                               │
└────────────────────────────┬─────────────────────────────────────────────┘
                             │
         ┌───────────────────┼───────────────────────┐
         │                   │                       │
┌────────▼────────┐ ┌────────▼────────┐   ┌──────────▼──────────┐
│   actions.h     │ │   replay.h /    │   │  replay_saver.h     │
│ action_state,   │ │  replay_saver.h │   │  replay_saver_state │
│ action_functions│ │  (replay I/O)   │   │  (live recording)   │
│ Depends: bwgame │ │ Depends: bwgame │   │ Depends: bwgame     │
└────────┬────────┘ └─────────────────┘   └─────────────────────┘
         │
┌────────▼─────────────────────────────────────────────────────────────────┐
│                          bwgame.h  (compatibility facade)                │
│   Includes bwgame_tables.h + bwgame_state.h, then defines:              │
│     state_functions  – all per-frame simulation logic (~19 000 lines)   │
│     state_copier     – snapshot / restore of state_base_copyable        │
│     game_load_functions – map loading, type-table construction           │
│     string_table_data   – Korean/localisation string tables             │
│     game_player      – top-level owner: holds global/game/state ptrs    │
│   Depends: bwgame_tables.h, bwgame_state.h, data_loading.h, korean.h   │
└──────────────────────┬───────────────────────────────────────────────────┘
                       │
        ┌──────────────┼──────────────────┐
        │              │                  │
┌───────▼───────┐ ┌────▼──────────┐ ┌────▼──────────────┐
│bwgame_state.h │ │bwgame_tables.h│ │  data_loading.h   │
│ (Phase 1 new) │ │ (Phase 1 new) │ │  (binary readers) │
│               │ │               │ └───────────────────┘
│ global_state  │ │ tan_table     │
│ game_state    │ │ direction_table│
│ state_base_   │ │ repulse_adjust│
│  copyable     │ │ psi_field_mask│
│ state_base_   │ │ all_player_   │
│  non_copyable │ │  slots  …     │
│ state         │ └───────────────┘
└───────┬───────┘
        │
┌───────▼────────────────────────────────────────────────────────────────┐
│  game_types.h   data_types.h   bwenums.h   util.h   containers.h      │
│  (primitive types: unit_t, bullet_t, tile_t, fp8, xy, intrusive_list, │
│   object_container, type_indexed_array, enums …)                       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Module Descriptions

### Primitive / Utility Layer

| Header | Purpose |
|---|---|
| `util.h` | Fixed-point arithmetic (`fp8`, `fp1`), `xy_t<T>`, string/format utilities, `a_vector`, `a_list`, `a_string`, `optional` |
| `containers.h` | Aggregate typedef for custom container types |
| `static_vector.h` | Fixed-capacity vector (no heap allocation; determinism-safe) |
| `circular_vector.h` | Ring-buffer vector |
| `intrusive_list.h` | Intrusive doubly-linked list (zero allocation; used for all live-entity lists) |
| `bwenums.h` | Scoped-like integer enums for all game entity types (`UnitTypes`, `WeaponTypes`, `UpgradeTypes`, `TechTypes`, `Orders`, `FlingyTypes`, `SpriteTypes`, `ImageTypes`) |
| `data_types.h` | `type_id<T>`, `type_container<T>`, `type_indexed_array<V,E>`, forward declarations for all DAT-backed type structs |
| `game_types.h` | Full definitions for `unit_t`, `bullet_t`, `flingy_t`, `sprite_t`, `image_t`, `order_t`, `path_t`, `thingy_t`, tile types, region types, trigger types |

### Data / Asset Loading

| Header | Purpose |
|---|---|
| `data_loading.h` | Endian-safe binary readers, DAT file loaders (`load_flingy_dat`, `load_sprites_dat`, …), CRC32 |
| `korean.h` | Korean character mapping tables and string conversion |

### Engine Core

| Header | Purpose |
|---|---|
| `bwgame_tables.h` | **Phase 1.** Static offline-computed lookup tables: `tan_table`, `direction_table` (256 fp8 unit vectors), `repulse_adjust_table`, `cardinal_direction_xy`, `hit_near_target_positions`, `psi_field_mask`, `all_player_slots`. Annotated with provenance. |
| `bwgame_state.h` | **Phase 1.** Ownership-documented state types: `global_state` (immutable assets), `game_state` (per-match config), `state_base_copyable` (snapshottable frame state), `state_base_non_copyable` (intrusive entity pools), `state`. Also `autocast<T>` helper. |
| `bwgame.h` | Compatibility facade: includes `bwgame_tables.h` + `bwgame_state.h`, then defines `state_functions` (all simulation logic), `state_copier`, `game_load_functions`, `string_table_data`, `game_player`. |

### Action / Replay

| Header | Purpose |
|---|---|
| `actions.h` | `action_state`, `action_functions` — parses and dispatches in-game player commands; drives `state_functions::next_frame()` |
| `replay.h` | Replay file reader; feeds recorded action streams back into `action_functions` |
| `replay_saver.h` | `replay_saver_state` — accumulates live action data for writing a replay file |

### Network Sync

| Header | Purpose |
|---|---|
| `sync_protocol.h` | **Phase 1.** Wire-stable message type IDs (`sync_messages::id_*`) extracted from `sync.h`. Stable integer assignments; add new ids at the end only. |
| `sync.h` | `sync_state`, `sync_functions`, `sync_server_noop` — latency-buffered multi-client action scheduling, insync hash checks, lobby flow, BWAPI-compatible frame stepping. |

### UI / Platform

| Header / File | Purpose |
|---|---|
| `ui/ui.h` | SDL2-centric rendering, input, and sound integration; ~1 800 lines |
| `ui/sdl2.cpp` | SDL2 backend implementation |
| `ui/native_window.h` | Thin window-lifecycle interface |
| `ui/native_window_drawing.h` | Drawing surface interface |
| `ui/native_sound.h` | Sound playback interface |

### Integration Shim

| File | Purpose |
|---|---|
| `mini-openbwapi/openbwapi.cpp` | BWAPI-compatible C API wrapper over `game_player` / `action_functions` |
| `mini-openbwapi/BWAPI/` | BWAPI header stubs used by bot code |

---

## Ownership and Lifetime Model

```
game_player
 ├─ unique_ptr<global_state>   ← immutable after global_init(); shared as const*
 ├─ unique_ptr<game_state>     ← mutable at map-load time; effectively read-only
 │                                during simulation
 └─ unique_ptr<state>          ← full per-frame mutable state
      ├─ state_base_copyable   ← can be snapshot / restored by state_copier
      │    ├─ const global_state*  global  (points into game_player's unique_ptr)
      │    └─ game_state*          game    (points into game_player's unique_ptr)
      └─ state_base_non_copyable ← intrusive entity pools; not copyable
```

Key invariants:
- `global_state` is **never mutated** once `global_init()` returns.
- `game_state` is **effectively immutable** once `game_load_functions::load_map_file()` returns.  Treating it as read-only during simulation is a correctness requirement.
- `state_base_copyable` is the boundary for deterministic replay: only fields in this struct (plus the non-copyable pools that back them) participate in frame-hash checks.

---

## Dependency Graph (simplified include order)

```
util.h
  └── bwenums.h (no deps beyond util)
        └── data_types.h
              └── game_types.h
                    └── bwgame_tables.h  (util.h only)
                    └── bwgame_state.h   (util + data_types + game_types + bwenums)
                          └── bwgame.h   (facade: tables + state + data_loading + korean)
                                └── actions.h
                                      └── replay.h / replay_saver.h
                                            └── sync_protocol.h (no deps)
                                                  └── sync.h
```

---

## Phase 1 Changes Summary

| New / Changed file | What changed |
|---|---|
| `bwgame_tables.h` | **New.** Extracted static lookup tables; annotated with provenance comments. |
| `bwgame_state.h` | **New.** Extracted state type definitions; added ownership model documentation. |
| `sync_protocol.h` | **New.** Extracted `sync_messages` namespace with wire-format stability contract. |
| `bwgame.h` | Converted to compatibility facade (includes new headers; existing consumers unaffected). |
| `sync.h` | Replaced inline `sync_messages` enum with `#include "sync_protocol.h"`. |

All downstream consumers that previously included only `bwgame.h` or `sync.h`
continue to compile without modification.

---

## Future Phase Targets

| Phase | Focus area |
|---|---|
| Phase 1 (remaining) | Further `bwgame.h` splits: unit/order domain, combat/math helpers, map/region helpers |
| Phase 2 | Sync/replay observability: structured desync logging, replay format versioning |
| Phase 3 | UI platform abstraction, headless mode, runtime debug overlays |
| Phase 4 | Strong typing (scoped enums, typed IDs), serialization hardening |
