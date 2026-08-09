# Documentation Layout

The project documentation is split into two folders:

1. `docs/official`
   - Current user-facing universal documentation.
   - Versioned official game docs and change summaries.

2. `docs/unofficial`
   - Historical design notes, side docs, code-review instructions, probes, and working version logs.
   - The Codex architecture review source PDF lives here with its metadata.

For Ver0.3.7, this release intentionally excludes `docs/official`. Those files
remain a separately requested user-facing documentation-freeze series.

The release publishes coherent bounded monthly Decision snapshots for all
alive civilizations, redesigns the Stability presentation, persists and shows
the three newest completed principal wars, and unifies ocean texture scale and
coordinates inside and outside the playable map. The no-world viewport uses
the same ocean texture without generated-world decoration. The save format is
MAP21; MAP20 and older files are rejected without migration.

Gameplay formulas, war outcomes, Decision formulas, world generation, climate,
hydrology, routes, plague, diplomacy classification, and simulation-speed
semantics are not retuned. Ver0.3.7 passed the ordered Rule 47 prerequisites
and a fresh final-source Rule 39 acceptance run on 2026-08-09. MAP20 and older
saves remain intentionally unsupported.

The non-official release records are:

- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.7_side_doc.md`

The previous climate-calibration, world-generation-control, static-geography,
and named-plague release records remain at:

- `docs/unofficial/ver0.3.6.c_side_doc.md`
- `docs/unofficial/ver0.3.6.b_side_doc.md`
- `docs/unofficial/ver0.3.6.a_side_doc.md`
- `docs/unofficial/ver0.3.6_side_doc.md`

The working version log is:

- `docs/unofficial/version_log.md`
