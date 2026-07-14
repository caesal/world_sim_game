# Ver0.3.6 Side Doc

Ver0.3.6 is the named global plague and Plague-panel release over Ver0.3.5.g.

This release intentionally does not update `docs/official`; that directory is
the separately requested official documentation-freeze series. The release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.6`.
2. Replaced the previous monthly city-roll plague mechanics with one global,
   bilingual named plague episode at a time.
3. Added 20-year scheduled checks, a rolling three-starts-per-100-year cap,
   fixed episode severity, frozen spore budgets, bounded six-month decisions,
   batched spread commits, and no same-pulse cascade.
4. Reused cached land and SeaLane adjacency plus existing deep-route eligibility
   for route-weighted spread without tile or all-city-pair plague scans.
5. Added exact monthly-equivalent mortality, age-vulnerability allocation that
   does not increase total deaths, duration-based immunity, and quarterly
   plague disorder.
6. Added 100 unique bilingual plague names before Roman-numeral reuse and
   episode-aware structured start/end World Announcements.
7. Rebuilt the Plague panel around fixed fog/probability controls and Live,
   Impact, and History tabs. Impact prioritizes countries and then the five
   highest-death cities. History compares the seven latest completed episodes.
8. Added linked integer No plague/Small/Medium/Large probability controls with
   exact-total validation, Reset, immediate inactive Apply, and persisted
   post-episode Apply while a plague is active.
9. Improved country-color identity blocks, square fitted metric icons, compact
   paging, spore wording, bilingual layouts, and shared overlay alpha.
10. Increased `MAP_SAVE_VERSION` to 19 and persisted complete plague episode,
    immunity, history, name-cycle, mortality-carry, event-identity, and
    effective/pending probability state.

## Approved Rules

- Checks start at Year 20 Month 1 and repeat every 20 years. An active episode
  consumes and skips its scheduled check rather than postponing it.
- At most three outbreak starts may occur in any rolling 100-year window;
  zero outbreaks remain possible.
- Default event probabilities are 50 percent none, 25 percent small,
  15 percent medium, and 10 percent large.
- Small/Medium/Large use 25/55/70 percent spore budgets, maximum generations
  4/6/8, continuous infection caps 48/72/144 months, and spread/persist bases
  55/45, 65/35, and 75/25.
- Episode severity is fixed and uniformly selected from 1-4, 5-8, or 9-10.
  It controls only annual mortality, from 6 to 15 percent.
- Initial infections last 24-42 months. Each active city receives one decision
  pulse every six months; persistence consumes one spore and adds 12 months.
- Pre-deep route weights are 65/35 land/shallow. After existing deep-route
  eligibility unlocks, weights are 50/30/20 land/shallow/deep and renormalize
  over available route types.
- Episode-duration immunity is 30 percent below 60 months, 50 percent from
  60-119, 80 percent from 120-239, and 100 percent from 240 months. Immunity
  lasts 480 months and changes infection selection, not breakthrough mortality.
- Plague disorder uses infected population share, severity-scaled infected
  share, and rolling 12-month plague death share, with bounded quarterly moves.
- Fog remains presentation-only, defaults to 50, and maps `0/50/100` to the
  prior effective `0/80/120` strengths.

## Save And Compatibility

- `MAP_SAVE_VERSION` is `19`.
- Version 18 is rejected before interpreting its dynamic payload, using the
  existing localized incompatibility path. No migration is attempted.
- The plague block uses an explicit internal version and validates effective
  and pending probability distributions strictly instead of normalizing
  malformed data.
- Effective and pending outbreak probabilities, pending validity, active
  episode state, histories, immunity, rolling starts/deaths, names, carries,
  and structured event identity all round-trip.

## Architecture Boundaries

- Scheduler, episode lifecycle, state, probability rules, adjacency, spread,
  immunity, mortality, disorder, names, save, announcements, snapshot, panel,
  UI input, and probes are split by responsibility.
- Rendering reads `RenderSnapshot` or cached presentation models and does not
  mutate simulation state.
- UI probability Apply crosses a game request boundary; it does not rewrite
  simulation state directly.
- Plague spread uses bounded active lists, cached adjacency, and pending commits.
  It adds no per-frame tile scan or all-city-pair scan.
- Both `Makefile` and `build.bat` contain the same source inventory. No C source
  includes another C source, and every C/H file remains at or below 500 lines.

## Validation Evidence

- Canonical Make build and `build.bat`: passed.
- Named-plague model probe: 58/58 passed, including 1,414,808 linked-slider
  endpoint and invariant cases.
- Presentation probe: `overall_ok=1`, including English/Chinese, 340/460 widths,
  fog, probability controls, country blocks, icons, paging, all tabs, charts,
  and structured announcements.
- 1,000-city stress: 124 microseconds average and 805 microseconds peak plague
  step, with zero tile and zero all-city-pair scans.
- Targeted non-activating GUI lifecycle: passed, including pending Apply,
  save/reload, natural episode end, promotion, and fog states.
- Matched Extreme non-plague fixture: mean 32.812ms, median 28.910ms,
  p95 64.707ms, and peak 235.644ms, all improved from the pre-edit sample.
- Fresh Rule39: Extreme 1152x800, 26 initial civilizations, 1,082 natural
  regions, max/5x, final Year 742 Month 2, and 31 civilizations at stage 5+.
- First five stage-5 qualifiers: `0 Kraheim`, `1 Tibervia`, `2 Sowon`,
  `3 Jingcheng`, and `4 Dragor`.
- Routes changed from `0 total / 0 shallow / 0 deep` before unlock to
  `68 / 66 / 2` after unlock.
- Generated and late maps retained current province fill, borders, 1,075 city
  icons, labels, routes, and selected-country extent. Two 60-frame samples had
  zero anomalies, with zero dropped months and zero order skips.

## Residual Risk

- The v19 plague block intentionally rejects its unreleased earlier internal
  layout rather than adding migration code.
- Plague view refresh still performs bounded city-state aggregation when its
  dirty key changes. It does not run every frame, and matched/stress evidence
  shows no task-specific performance regression.
- The late Rule39 stop recorded 213ms/month, 68ms render average, and a 693ms
  sampled render peak. Rule39 flow and matched task gates passed, but these
  numbers are not a claim of one universal performance ceiling across every
  generated world and machine.
