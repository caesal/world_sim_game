# Documentation Layout

The project documentation is split into two folders:

1. `docs/official`
   - Current user-facing universal documentation.
   - Versioned official game docs and change summaries.

2. `docs/unofficial`
   - Historical design notes, side docs, code-review instructions, probes, and working version logs.
   - The Codex architecture review source PDF lives here with its metadata.

For Ver0.3.7.a, this release intentionally excludes `docs/official`. Those
files remain a separately requested user-facing documentation-freeze series.

The implemented Ver0.3.7.a areas are episode-total plague-immunity thresholds
and episode-wide assignment, map-size-aware Initial Civilizations controls, the
fixed diminishing aridity response with its strict oasis window, and plague
RenderSnapshot/cache consistency across unrelated Decision and war-history
publication. Real plague, lane, city, month, civilization-source, and
world-generated changes continue to invalidate plague payloads. The coherent
Decision, Stability, completed-war, and ocean-presentation work from Ver0.3.7
remains in place. Ordinary pause now closes new-month admission, drains any
already-started month through coherent snapshot/front publication, and defers a
resume requested during that drain. The collapsed top-bar mode lane now uses
472 pixels so that all six labels, including English `Geography`, fit at the
required viewport without changing strings, fonts, or narrow-window fallback.
`MAP_SAVE_VERSION` remains 21; MAP20 and older files are rejected without
migration.

Final Rule 47 and Rule 39 acceptance, including manual visual review, passed on
2026-09-04. The run ended at Year 672 Month 5 with 1,212 natural regions,
64 initial and 64 surviving civilizations, and 1,443 wars started. The
[Ver0.3.7.a side document](unofficial/ver0.3.7.a_side_doc.md) records the five
stage-5 witnesses, final executable identity, and acceptance evidence hashes.
Git publication is a separately authorized operation whose outcome is verified
from the commit, lightweight tag, and remote references, not inferred from PASS.

The non-official release records are:

- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.7.a_side_doc.md`
- `docs/unofficial/ver0.3.7_side_doc.md`

The previous climate-calibration, world-generation-control, static-geography,
and named-plague release records remain at:

- `docs/unofficial/ver0.3.6.c_side_doc.md`
- `docs/unofficial/ver0.3.6.b_side_doc.md`
- `docs/unofficial/ver0.3.6.a_side_doc.md`
- `docs/unofficial/ver0.3.6_side_doc.md`

The working version log is:

- `docs/unofficial/version_log.md`
