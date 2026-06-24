# Ver0.3.4 Side Doc

Ver0.3.4 is an alliance lifecycle, diplomacy presentation, worldgen
performance, and UI polish checkpoint over Ver0.3.3.g. It keeps the
first-class Alliance panel baseline, then hardens the candidate/vote/history
records, 5x presentation path, diplomacy snapshot cache, and release validation
evidence.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.4`.
2. Optimized Extreme-map route-potential generation and added worldgen timing
   probe evidence.
3. Added immediate pressed/selected feedback for speed and top map-view
   buttons before simulation ticks.
4. Standardized map legend backgrounds at 65 percent opacity while keeping
   legend text, swatches, and icons opaque.
5. Added alliance diplomatic contact for candidate qualification,
   relation-score growth, join voting, and Alliance UI progress explanations.
6. Reworked Alliance Votes and History into event-card style records with
   candidate countdowns, previous real vote results, member result boxes, and
   retryable vote wording.
7. Implemented automatic 800-year alliance union: formal members merge into a
   new independent country, old members are absorbed, the alliance dissolves,
   and structured event/history/notification records are emitted.
8. Fixed diplomacy snapshot starvation so live contacts publish into
   RenderSnapshot without waiting for irrelevant dead civilization pairs.
9. Fixed Country Detail diplomacy/war cache keys so visible war fields update
   while open without reintroducing broad monthly panel rebuilds.
10. Fixed Alliance list badge alignment, no-alliance War/Peace badges, alliance
    legend ordering, stale cross-alliance applications, 30/10-year join-vote
    timing, and waiting-next-vote progress refresh.
11. Updated the root README, documentation index, version log, this side doc,
    active version marker, and in-game pause-menu version summary.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/game_notifications.*`
- `src/core/render_snapshot*`
- `src/core/version.h`
- `src/game/game*probe*`
- `src/main.c`
- `src/render/panel_alliance*`
- `src/render/panel_diplomacy_cache_key.*`
- `src/render/panel_info.c`
- `src/render/panel_map.c`
- `src/render/panel_view_model_cache.c`
- `src/render/render*`
- `src/sim/alliance*`
- `src/sim/diplomacy*`
- `src/sim/route_potential.*`
- `src/sim/sea_nav.*`
- `src/sim/war*`
- `src/ui/ui*`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.4_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `15`; no new save-version bump is part of this
  release metadata update beyond the existing alliance record serialization.
- Alliance union is now an implemented simulation behavior after the 800-year
  eligibility point. It does not introduce military-alliance or defensive-war
  response mechanics.
- Alliance diplomatic contact is intentionally limited to alliance candidate
  qualification, relation-score growth, join voting, and UI explanations. It is
  not used for war reachability, border friction, trade routes, ports,
  adjacency, resources, or map exploration.
- Map ownership/fill semantics are not changed by this release.

## Validation

Release validation for this checkpoint included:

- `make -B world_sim.exe` passed.
- `cmd /c build.bat` passed.
- `git diff --check` passed with only line-ending warnings where applicable.
- `.c` include scan found no source file including another `.c` file.
- `make check-text` passed.
- Touched `.c/.h` line counts were at or under the 500-line limit.
- Touched-file mojibake marker scan passed.
- `world_sim.exe --probe-presentation` passed with `overall_ok=1`.
- `world_sim.exe --probe-diplomacy` passed with `overall_ok=1`.
- `world_sim.exe --probe-worldgen` passed with Extreme-map route timing output.
- Root executable inventory contained exactly `world_sim.exe`.

Full Rule39 evidence for the Ver0.3.4 release stack is recorded under:

- `build/validation/ver0.3.4_rule39/`

Recorded Rule39 summary:

- Final year/month: `1277/9`.
- Natural regions: `1077`.
- Initial placed civilizations: `26/26`.
- Final civilization slots: `59`.
- Speed: max/5x.
- Five stage-5-or-higher civilizations:
  - `0 Dragoncrown Empire stage 10`
  - `1 The ZhaoLan Mandate stage 10`
  - `2 MuKuro Mandate stage 9`
  - `3 Stormveil Kingdom stage 10`
  - `4 Sunfall Empire stage 10`
- Deep-sea route evidence: before unlock `deep 0`; after unlock `deep 40`.

## Residual Risks

- The release stack is large and covers UI, diplomacy presentation, alliance
  lifecycle, worldgen performance, and rendering/presentation caches. Future
  fixes should stay narrow and retain the focused probes added in this line.
- The Rule39 run is executable evidence for the release stack; future version
  metadata-only edits should still preserve the tag on the final metadata
  commit.
