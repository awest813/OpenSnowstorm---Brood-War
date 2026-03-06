# Phase 4 Kickoff: Complete Client Experience Foundations

This document defines the first execution slice for **Phase 4** from
`docs/modernization-roadmap.md`: moving from an engine-focused milestone to a
usable, end-to-end Brood War client foundation.

## Scope for this kickoff

Phase 4 is broad, so this kickoff intentionally narrows work to a deterministic,
verifiable set of user-visible milestones:

1. **Single-player UX baseline hardening**
   - Stabilize map-session startup and in-game control paths for local play.
   - Prioritize campaign-critical interactions (selection, command issue,
     fog/vision consistency, pause/resume flows).
2. **Campaign-readiness gap closure (foundation only)**
   - Identify and prioritize missing trigger/briefing/save behaviors that block
     full campaign progression.
   - Add explicit compatibility checkpoints for those behaviors.
3. **Compatibility gating for user-facing flows**
   - Extend replay/hash validation so client-facing regressions are caught early.
   - Keep all new checks CI-friendly and deterministic.

## Why this is the right next phase

- Phases 1–3 established decomposition, replay/sync diagnostics, and CI
  baselines; those were prerequisites for product-level client work.
- The project now has a landed single-player foundation in `gfxtest --map`, so
  the highest leverage move is converting it from "demo-capable" to
  "campaign-usable".
- Phase 4 progress must remain measurable. Every user-facing improvement should
  be coupled to a repeatable validation command or fixture.

---


## Progress update (in progress)

- Added a replay seek-state resilience fix in `ui/gfxtest.cpp` to prevent early
  replay-frame seek/rewind paths from dereferencing an empty saved-state map.
- Extended `docs/broodwar-compatibility.md` with a Phase 4 campaign-readiness
  tracker (triggers, briefing flow, save/load) including deterministic commands
  that define pass/fail once fixtures are landed.
- **Completed BW action ID coverage (55–91)**: actions 59, 63–69, and 72–86 now
  have explicit skip entries in `actions.h`, eliminating stderr warning spam for
  lobby/system actions encountered in replays.
- **Stim pack** (`i` hotkey, command panel `ability_stim`) now available for
  Marine/Firebat in live map mode via `live_command_can_stim`.
- **Archon / Dark Archon merge** (`m` hotkey, command panel) now available for
  High Templar / Dark Templar when the relevant tech is researched.
- **Tab: center camera on selection** — pressing Tab in live map mode centers the
  viewport on the centroid of the current selection.
- **Multi-selection tactical command panel parity**: when multiple local units are
  selected, the command panel now exposes tactical controls (stop, hold,
  attack-move mode, patrol mode) without requiring hotkeys; attack/patrol mode
  selection continues to apply to the next right click target.
- **Terran lift off / land + Ghost cancel-nuke parity**: live map mode now
  exposes a context-sensitive `l` path (unload-all first, then lift off / land
  for Terran flying-building structures), landing uses armed placement flow, and
  cancel (`x` / command panel) now covers active nuclear launch cancellation.
- **Targeted spell ability surface (25 spells)**: Science Vessel (scanner sweep,
  defensive matrix, irradiate, EMP), Battlecruiser (yamato gun), Ghost (lockdown),
  Vulture (spider mines), Medic (heal, restoration, optical flare), Queen (spawn
  broodlings, parasite, ensnare, infestation), Defiler (dark swarm, plague,
  consume), High Templar (psionic storm, hallucination), Arbiter (recall, stasis
  field), Dark Archon (mind control, feedback, maelstrom), Corsair (disruption web)
  — all now appear in the command panel and arm a `pending_order_mode` spell-
  targeting mode; right-click fires the spell, ESC cancels.
- **`--headless-map` mode**: runs a map session headlessly for up to N frames
  (default 72000) and exits cleanly — enables CI smoke tests without a display.
- **`--gen-test-replay` fixture generator**: generates a self-contained BW-format
  replay from any map file, enabling creation of `maps/test.rep` + `maps/test.hashes`
  for the CI gate without needing a live game session.
- **Debug overlay (F3 / `--debug-overlay`)**: pressing F3 or passing `--debug-overlay`
  shows a compact top-left HUD with current frame number, draw FPS, game speed
  multiplier and a paused indicator; works in both replay and live-map modes.
  `fps_draw_last` is updated once per second in `ui_functions::update()`.
- **Desync action-history ring buffer**: `desync_report` now carries a 16-entry
  ring buffer of the most-recently executed local actions at mismatch time; field
  names `recent_actions`, `recent_actions_count`, `recent_actions_tail` with a
  convenience `push_recent_action()` method.  `sync_functions` overrides
  `on_action()` to push to `sync_state::recent_action_ring` on every dispatched
  action; the buffer is snapshotted into the `desync_report` at mismatch
  detection.  `write_desync_reports()` prints the history oldest-first for
  structured CI/log triage.
- **Quicksave / quickload (F5 / F8)**: single-player live map mode now supports
  an in-memory save slot. Pressing F5 deep-copies the current `state`,
  `action_state`, and APM counters into `main_t::quicksave_slot` via the existing
  `copy_state` infrastructure; F8 restores from that slot and resets the
  victory/defeat latch so result detection fires correctly after a reload.
  The game auto-pauses when victory or defeat is detected, giving the player a
  moment to react before resuming or reloading.
  **F5/F8 now also push HUD messages** ("Saved.", "Loaded.", "No save.") so players
  receive on-screen feedback without needing a console.
- Remaining immediate slice: generate and commit `maps/test.rep` + `maps/test.hashes`
  using `--gen-test-replay` to activate the CI `validate-replay` gate.
- **Trigger system expansion (Phase 4 continuation)**: Substantially expanded the
  StarEdit trigger engine in `bwgame.h`:
  - **New state fields** (`bwgame_state.h`): `switches[256]` (global trigger switch
    flags), `countdown_timer` (global countdown in game seconds), and
    `unit_deaths[12][228]` (per-player per-unit-type death counters).
  - **Countdown timer decrement**: `process_triggers` now decrements
    `st.countdown_timer` once per ~24 frames (~1 game second), matching BW's
    gameplay timer.
  - **Death tracking**: `destroy_unit_impl` now increments `st.unit_deaths[owner][uid]`
    whenever a unit is fully destroyed, enabling the Deaths/Kill trigger conditions.
  - **New trigger conditions**: Countdown Timer (1), Accumulate resources (4),
    Kill (5, mapped to deaths), Switch state (11), Mission brief no-op (13),
    Deaths (15), Score (21), Never (23).  Unknown condition types now return false
    gracefully instead of throwing.
  - **New trigger actions**: Pause Game (5), Unpause Game (6), Transmission (7),
    Play Sound (8), Display Text Message (9, fires `on_trigger_display_text`),
    Center View (10, fires `on_trigger_center_view`), Create Unit with Properties (11),
    Set Mission Objectives (12, fires `on_trigger_set_objectives`), Move Unit to
    Location (13), Set Alliance Status (16), Set Score (17), Set Countdown Timer
    (21 and alternate 59), Kill Unit at Location (23), Leaderboard no-ops (27–31,
    61–64), Draw (32), Give Units to Player (36), Set Switch (40, supports set/clear/
    toggle/randomize), Modify Unit HP (48), Modify Unit Energy (49), Modify Unit
    Shield Points (50), Modify Unit Resource Amount (51), Minimap Ping (55),
    Set Next Scenario (58, fires `on_trigger_set_next_scenario`).  Unknown action
    types are silently skipped (no-op) instead of crashing.
  - **Bug fix**: trigger action 26 (Set Resources) subtract path previously used
    `num_n == 8` (same as add, unreachable); corrected to `num_n == 9`.
  - **Helper refactor**: extracted `trigger_unit_matches_filter()` from duplicated
    inline logic in kill/remove/order/give actions.  `get_map_string()` is now
    available in `state_functions` so trigger actions can access mission text
    without going through `game_load_functions`.
  - **Trigger event callbacks**: added `on_trigger_display_text`,
    `on_trigger_transmission`, `on_trigger_center_view`, `on_trigger_set_objectives`,
    `on_trigger_set_next_scenario` virtual hooks to `state_functions`; the UI layer
    can override these to surface mission text and camera cues.
- **Trigger callbacks wired to `ui_functions` (campaign playability)**:
  - `on_trigger_pause_game` / `on_trigger_unpause_game` virtual hooks added to
    `state_functions`; trigger actions 5/6 now call them; `ui_functions` overrides
    to actually pause/resume the simulation — closes a hard campaign-blocking gap.
  - `on_trigger_display_text`, `on_trigger_transmission` push on-screen HUD banners
    via `push_hud_message()`; players now see mission text during campaign play.
  - `on_trigger_center_view` scrolls the viewport to the trigger's location —
    scripted camera cues now work correctly.
  - `on_trigger_set_objectives` logs mission objectives; feeds a future objectives
    HUD panel.
  - `on_trigger_set_next_scenario` stores the transition target in
    `ui_functions::pending_next_scenario`; the gfxtest main loop logs it at
    mission victory; the Emscripten layer exposes it via `replay_get_value(7)`.
  - `on_victory_state` in `ui_functions` auto-pauses the game for the local player
    and pushes a "Mission accomplished." / "Mission failed." banner, ensuring
    trigger-driven win/loss states are properly surfaced to the player.
- **HUD text message overlay**: `draw_hud_messages()` renders up to 4 timed text
  banners at the bottom-centre of the viewport.  Trigger text, save/load feedback,
  and victory/defeat banners all use this path.
- **`load_map_data` Emscripten entry point**: new `extern "C" void load_map_data(...)`
  allows the JS/browser layer to load a campaign map from raw bytes into a live
  interactive single-player session, closing the gap that previously limited the
  browser build to replay-only mode.
- **`replay_get_value(7/8)` + `replay_set_value(7, 0)`**: JS layer can now poll the
  pending-next-scenario string and local-player victory state, and clear the
  scenario pending flag after handling the transition.


---

## Kickoff workstreams

### Workstream A — Playable local session quality

| Slice | Files (expected) | Validation signal |
|---|---|---|
| Session startup/teardown resilience | `ui/gfxtest.cpp`, map/replay loading paths | `--map` boot completes and exits cleanly across a smoke set of maps. |
| Input command parity pass | `actions.h`, input translation paths | Command sequences replay without unknown-action warnings for covered IDs. |
| Local-vision correctness spot checks | UI render paths + fog/minimap code | Deterministic fixture pass with no fog/explored regressions. |

### Workstream B — Campaign foundation

| Slice | Files (expected) | Validation signal |
|---|---|---|
| Trigger coverage audit + priority list | `docs/broodwar-compatibility.md` + trigger implementation files | Tracker entries updated with status and concrete next checks. |
| Briefing flow scaffolding | UI/campaign control paths | Briefing entry/exit loop runs without soft-lock on selected maps. |
| Save/load foundation plan | save-state and replay/state docs | Defined schema/compat strategy with deterministic restore checks. |

### Workstream C — Regression gates

| Slice | Files (expected) | Validation signal |
|---|---|---|
| Fixture expansion for client flows | `maps/*.rep`, `maps/*.hashes`, CI workflow | CI executes replay/hash checks for at least one user-facing scenario. |
| Compatibility-report discipline | `docs/broodwar-compatibility.md` | Every landed Phase 4 item links to a commandable pass/fail check. |

---

## Exit criteria for Phase 4 kickoff

This kickoff is considered complete when:

1. At least one deterministic CI gate validates a user-facing local-play flow
   (not just raw parsing/build checks).
2. `docs/broodwar-compatibility.md` includes a campaign-readiness subsection
   with prioritized blockers and measurable checks.
3. A documented shortlist of campaign-critical missing behaviors exists with
   owners/next slices (triggers, briefing, save/load).

## Immediate next slices

1. Generate `maps/test.rep` + `maps/test.hashes` using
   `gfxtest --gen-test-replay maps/test.rep --map <any.scx> --frames 240 --record-hashes maps/test.hashes`
   and commit them to activate the CI `validate-replay` gate.
2. Add campaign-readiness tracker rows (triggers, briefing, save/load) with
   statuses and concrete validation commands.
3. Land one focused UX hardening patch that eliminates a known local-session
   failure mode and documents the check used to verify it.

## Non-goals for this kickoff

- Full campaign completion parity.
- Lobby/matchmaking feature completion.
- Renderer replacement or major backend migration.

Those remain within broader Phase 4/5 scope but are intentionally deferred until
this kickoff's deterministic quality gates are in place.
