# Ver0.3.4.a Side Doc

Ver0.3.4.a is a checkpoint over Ver0.3.4 before the next expansion/render
stutter pass. It freezes the current alliance lifecycle, forced war-defeat
alliance-exit, four-heritage country-name, and diplomacy tooltip stability
stack so the upcoming performance fix can start from a pushed baseline.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.4.a`.
2. Added war-defeat settlement behavior that can force a defeated formal
   alliance member out before cession or indemnity settlement.
3. Reused existing kicked-cooldown semantics so forced war-defeat exit blocks
   rejoining the same alliance for 100 years.
4. Added forced-breakup pair cooldown so two countries split by war-defeat
   alliance exit cannot immediately recreate the same two-member alliance loop.
5. Added structured Event Log support for forced war-defeat alliance exits,
   including loser, winner, and alliance identity instead of unknown-country
   fallback text.
6. Added alliance History entries for members removed because of military
   defeat, with preserved dissolved-alliance history display.
7. Reworked alliance slot/name allocation so inactive slots can be reused, but
   all 192 base alliance names are used once before a Roman suffix is applied
   to an individual reused base name.
8. Added Western, Eastern, Southern, and Northern country-name heritage pools
   with 200 bilingual names per pool.
9. Updated default civilization seeding so the four heritage pools are assigned
   evenly, with seed-randomized remainder slots when the civilization count is
   not divisible by four.
10. Kept same-heritage affinity equality-only: same heritage retains the
    existing cultural closeness effects, while all different-heritage pairs are
    treated alike.
11. Reused Western province names for Northern heritage and Eastern province
    names for Southern heritage.
12. Stabilized diplomacy tooltip hover data by separating build-time hits from
    committed active hits and scoping Country Diplomacy vs Alliance Votes hover
    tests.

## Files In Scope

- `Makefile`
- `build.bat`
- `data/country_names_*_200_bilingual.tsv`
- `src/core/event_log*`
- `src/core/game_types.h`
- `src/core/sim_types.h`
- `src/core/version.h`
- `src/data/country_names.c`
- `src/data/province_names.c`
- `src/game/game*probe*`
- `src/render/panel_alliance*`
- `src/render/panel_country_diplomacy*`
- `src/render/panel_country_events.c`
- `src/render/panel_debug.c`
- `src/render/panel_view_model_cache.c`
- `src/sim/alliance*`
- `src/sim/civilization_names.c`
- `src/sim/diplomacy.c`
- `src/sim/simulation*`
- `src/sim/war_resolution.c`
- `src/ui/ui*`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.4.a_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `15`.
- The checkpoint intentionally does not include the upcoming expansion-phase
  stutter fix. Early expansion, political fill, border cache, city overlay, and
  label cache performance remain the next diagnosis/fix scope.
- Forced war-defeat alliance exit is a gameplay rule change in this checkpoint.
  It is intended to happen before cession and indemnity; if it succeeds, the
  defeated country exits its alliance and the cession/indemnity settlement path
  is skipped.
- Severe collapse/disorder vassalization thresholds remain available through
  the existing war-resolution logic.
- Four heritage pools are name/culture pools only. They are not bound to map
  geography.

## Validation Notes

- Canonical `make -B world_sim.exe` and `cmd /c build.bat` were attempted first
  but could not overwrite the running locked executable.
- Temporary-target `make` validation passed.
- Temporary-output `build.bat` validation passed, and the temporary output was
  deleted.
- `git diff --check` passed with line-ending warnings only.
- `make check-text` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched `.c/.h` line counts were at or under the 500-line limit.
- Touched-file mojibake marker scan passed.
- `world_sim_validation_tmp.exe --probe-presentation` passed with
  `overall_ok=1`.
- `world_sim_validation_tmp.exe --probe-diplomacy` passed with `overall_ok=1`.
- `world_sim_validation_tmp.exe --probe-worldgen` passed with `overall_ok=1`.
- The earlier Ver0.3.4 Rule39 folder is historical evidence for Ver0.3.4 only.
- Do not claim full Ver0.3.4.a release-ready acceptance unless a fresh Rule39
  run is completed for this checkpoint.

## Residual Risks

- The user-reported early-expansion stutter remains open. The likely hot path
  is repeated expansion-claim invalidation of ownership/province/city/label
  revisions, causing full-map presentation and cache rebuilds during the first
  expansion-heavy years.
- Full GUI/Rule39 validation should be rerun after the expansion/render stutter
  fix, with Debug / Performance evidence from the expansion phase itself.
