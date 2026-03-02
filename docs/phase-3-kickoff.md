# Phase 3 Kickoff: Platform / CI Hardening and BW Compatibility Completions

This document tracks the first vertical slices for **Phase 3** from
`docs/modernization-roadmap.md`, together with the remaining **Phase 2** items
that were deferred from the previous kickoff.

## Scope for this kickoff

Two parallel streams are active:

1. **Phase 2 completions** – items deferred from `phase-2-kickoff.md` that are
   prerequisites for safe Phase 3 work.
2. **Phase 3 foundations** – CI infrastructure and build unification that all
   subsequent phases depend on.

## Why these are the right next steps

- Without CI, any refactor or compatibility fix is unverified.  The CI
  baseline is the single highest-leverage item in the roadmap.
- Protocol version metadata and desync diagnostics are direct enablers for
  future sync changes — they must land before Phase 3 multiplayer work begins.
- Graceful action skipping prevents crashes on real-world BW replays that
  contain actions the engine hasn't seen before, unblocking the replay fixture
  plan.

---

## Changes landed in this kickoff

### Build infrastructure

| Change | Files | What it enables |
|---|---|---|
| Top-level `CMakeLists.txt` | `CMakeLists.txt` (new) | Single-command configure+build for both `gfxtest` and `mini-openbwapi` from the repository root. |
| `CMakePresets.json` | `CMakePresets.json` (new) | Named presets for Debug/Release × GCC/Clang + a `no-ui` headless preset; reduces setup friction for new contributors. |
| GitHub Actions CI | `.github/workflows/ci.yml` (new) | Linux matrix build: `{gcc, clang} × {Debug, Release}`.  A separate `build-no-ui` job verifies headless compilation without SDL2.  A `validate-replay` job template is included and enabled once a test replay is committed. |

### Protocol / diagnostics

| Change | Files | What it enables |
|---|---|---|
| `sync_protocol_min_peer_version` constant | `sync_protocol.h` | Explicit minimum acceptable peer version for handshake rejection; documents compatibility policy inline. |
| `write_desync_reports()` formatter | `sync_protocol.h` | Writes structured `key: value` desync diagnostics to any `FILE*` sink (stderr by default), making CI logs actionable without a debugger. |
| Automatic stderr desync emission | `sync.h` | On first insync mismatch, calls `write_desync_reports(stderr, …)` immediately before killing the diverging client. |

### BW compatibility

| Change | Files | What it enables |
|---|---|---|
| Graceful skip for unknown/unimplemented actions | `actions.h` | Unknown action IDs log a warning to stderr and consume remaining bytes in the frame chunk instead of crashing. |
| `read_action_skip<N>` helper template | `actions.h` | Zero-simulation skip for replay-observer-only actions with known payload sizes. |
| Actions 55–58, 60–62, 70–71, 89, 91 | `actions.h` | Save game, load game, restart, game-speed change, pause, resume, vision-toggle, allied-victory, and BW replay markers are now consumed without error. |

---

## Exit criteria for Phase 3 kickoff

This kickoff is considered complete when:

1. All four CI jobs (gcc/Debug, gcc/Release, clang/Debug, clang/Release) pass
   on the default branch.
2. `write_desync_reports` is exercised by at least one sync divergence test or
   doc example.
3. `--validate-replay` successfully processes a real BW replay without hitting
   the unknown-action error path (requires a replay in `maps/`).

---

## Replay validation gate

The CI `validate-replay` job is currently disabled (`if: ${{ false }}`).
To activate it:

1. Obtain a small, freely distributable BW replay (for example, a replay from
   an open tournament or one you recorded yourself).
2. Commit it to `maps/test.rep`.
3. In `.github/workflows/ci.yml`, change `if: ${{ false }}` to `if: true`.

Once active, any regression that breaks replay parsing will fail CI
automatically.

---

## Next slices

1. **Replay fixture with frame-hash checkpoints** – commit `maps/test.rep`,
   record golden hashes at N-frame intervals, add a `--verify-hashes` mode to
   `gfxtest` that asserts against them.
2. **Desync history in reports** – extend `desync_report` with a short ring
   buffer of recent action IDs for each player, surfaced in `write_desync_reports`.
3. **Platform window abstraction** – define a narrow `platform_window` interface
   so SDL2 is one backend, not the only option (Phase 3.1).
4. **Runtime config file** – `--config <file>` flag parsed in `gfxtest`; lets
   CI set data-path, window size, etc. without recompilation.
5. **Debug overlays** – FPS counter, current frame number, and sync-state
   indicator rendered in the SDL2 UI (Phase 3.2).
