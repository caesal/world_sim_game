# Documentation Layout

The project documentation is split into two folders:

1. `docs/official`
   - Current user-facing universal documentation.
   - Versioned official game docs and change summaries.

2. `docs/unofficial`
   - Historical design notes, side docs, code-review instructions, probes, and working version logs.
   - The Codex architecture review source PDF lives here with its metadata.

For Ver0.3.6.a, this release update intentionally excludes `docs/official`.
Those files remain a separately requested user-facing documentation-freeze
series. The current code release rebuilds static geography, wind, climate,
hydrology, lakes, rivers, immutable map presentation, low-ocean generation,
coast semantics, and river LOD; it advances the map save format to version 20.
The named-plague model and panel from Ver0.3.6 remain part of the same source
stack. Deterministic probes, boundary matrices, matched resource/cache checks,
targeted GUI validation, flicker checks, and final Rule39 evidence cover the
release.
The non-official release records are:

- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.6.a_side_doc.md`

The previous named-plague release record remains at:

- `docs/unofficial/ver0.3.6_side_doc.md`

The working version log is:

- `docs/unofficial/version_log.md`
