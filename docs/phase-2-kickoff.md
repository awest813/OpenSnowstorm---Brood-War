# Phase 2 Kickoff: Sync/Replay Reliability and Diagnostics

This document tracks the first vertical slice for **Phase 2** from `docs/modernization-roadmap.md`.

## Scope for this kickoff

Phase 2 targets reliability and diagnosability at the sync/replay boundary. For this kickoff, the practical objective is to define a shippable sequence instead of taking on all reliability work in one change.

## Why this is phase-2 aligned

- Builds directly on the protocol extraction started in Phase 1 (`sync_protocol.h`) as the boundary definition point.
- Prioritizes desync triage speed by making diagnostics an explicit deliverable for each slice.
- Keeps replay durability tasks attached to validation commands so compatibility progress remains observable.

## Initial slice plan

1. **Protocol surface hardening**
   - Keep protocol IDs centralized in `sync_protocol.h`.
   - Add/maintain documentation for message semantics and wire stability rules.

2. **Desync report artifact (minimum viable)**
   - Produce a compact report on first divergence that includes:
     - local frame,
     - reported hash/checkpoint index,
     - expected hash vs received hash,
     - client UID / slot when available.

3. **Replay validation pass**
   - Add a replay sanity check command that verifies header and frame stream consistency.
   - Store a short compatibility checklist in docs with command + expected result.

## Exit criteria for Phase 2 kickoff

The kickoff slice is considered complete when:

1. At least one deterministic sync divergence path emits a structured report.
2. At least one replay validity check is documented and runnable.
3. `docs/broodwar-compatibility.md` links to the checks as evidence for status progression.

## Next slices

1. Add message-level protocol version metadata and compatibility policy notes.
2. Extend desync reports with recent action/message history snippets.
3. Promote replay validity checks into CI as non-optional compatibility gates.
