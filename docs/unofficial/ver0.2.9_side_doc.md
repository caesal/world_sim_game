# Ver0.2.9 Side Doc

Ver0.2.9 is a stabilization release over Ver0.2.8. It focuses on event-log
authority, country-panel summary visibility, East/West naming data, stability
decision presentation, route/label/render cache refinement, and smaller UI
readability fixes across diplomacy, disorder, decision, and vassal cards.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.9`.
2. Added a canonical structured event store so the global Event Log is the
   authority and country recent events resolve through global event ids.
3. Added compact country-count and independent-country-count cards under the
   country panel title.
4. Added East/West civilization heritage support, bilingual country and
   province name pools, and heritage-aware generated naming.
5. Updated manual civilization randomization so heritage, name, metrics,
   symbol, and preview color refresh together, and custom HSV colors are used
   by added civilizations.
6. Replaced the UI icon asset mapping with the provided transparent strategy
   icon packs.
7. Added and refined stability-decision UI views for overview, expansion, war,
   and stability gates without changing the underlying simulation rules.
8. Improved single-province collapse handling, vassal card wording, decision
   countdown display, and disorder/territory-integrity presentation.
9. Continued route, label, plague, side-panel, and scene-cache diagnostics and
   cache-boundary tuning to reduce avoidable UI/render invalidation.

## Validation

- `make check-text` is required before the Ver0.2.9 commit.
- `git diff --check` is required before the Ver0.2.9 commit.
- `make` is required before the Ver0.2.9 commit.
- Existing probes should be run where practical; any timeout or skipped GUI
  game-flow validation must be recorded in the final release report.
