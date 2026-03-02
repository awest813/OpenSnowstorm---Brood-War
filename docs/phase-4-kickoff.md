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
- Remaining immediate slice: commit the first user-facing replay fixture
  (`maps/test.rep` + `maps/test.hashes`) and enable the CI replay gate.

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

1. Commit one minimal replay+hash fixture that exercises map start, unit
   selection, and at least one combat interaction in a local-play scenario.
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
