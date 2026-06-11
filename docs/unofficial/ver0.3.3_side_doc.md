# Ver0.3.3 Side Doc

Ver0.3.3 is a population, military, small-population, and technology pacing
checkpoint over Ver0.3.2.g. It accepts the display-only population pyramid work
from Ver0.3.2.g, keeps real gameplay storage on the existing 8
`PopulationCohort` bands, and adds clearer military/read-model presentation plus
longer technology stage pacing.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3`.
2. Added `src/sim/population_military.c/.h` for the population-derived army
   model and weighted casualty deduction.
3. Added `src/sim/population_aging.c/.h` so tiny real cohorts can eventually
   age forward without being stuck at integer zero.
4. Added `src/render/panel_country_population_cards.c/.h` for Country
   Population structure cards.
5. Added `src/game/game_population_corner_probe.c/.h` for focused army,
   small-population, aging-reset, single-civilization, and technology timing
   probes.
6. Updated Country Population structure cards to show:
   - `Children / Fertile / Elder`
   - `Workers / Recruitable / Army`
7. Shows `Workers`, `Recruitable`, and `Army` as male/female split values.
8. Removed the duplicated `Usage` card from the structure-card grid while
   keeping the top population/carrying-capacity usage bar.
9. Added fractional x100 diagnostics for expected births, total deaths, and net
   monthly population change so tiny populations do not flatten to `0/mo`.
10. Changed single-civilization auto-run behavior so auto-run stops at zero
    living civilizations, not one.
11. Updated technology stage duration to use a 120-year innovation-5 baseline,
    5 years per innovation point, larger resource/pressure modifiers, and an
    80..150 year final clamp.

## Files In Scope

- `src/core/version.h`
- `Makefile`
- `build.bat`
- `src/game/game.c`
- `src/game/game_population_probe.c`
- `src/game/game_population_corner_probe.c/.h`
- `src/game/game_worldgen.c`
- `src/render/panel_country_population.c`
- `src/render/panel_country_population_cards.c/.h`
- `src/render/panel_view_model_cache.c`
- `src/sim/population.c`
- `src/sim/population_aging.c/.h`
- `src/sim/population_diagnostics.c/.h`
- `src/sim/population_military.c/.h`
- `src/sim/simulation_month.c/.h`
- `src/sim/technology.c/.h`
- `src/sim/war.c`
- `src/ui/pause_menu.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3_side_doc.md`

## Behavioral Notes

- Real population storage still uses the existing 8 gameplay cohorts.
- Display-only yearly cohorts remain presentation and old-age mortality support;
  they do not become save-format or broad gameplay storage.
- The visible population pyramid keeps a single `75+` row and no visible `81+`
  row.
- Own current soldiers are derived from:
  `round((male_25_64 * 25 + female_25_64 * 10) / 1000) - active war casualties`,
  clamped at zero.
- `18-24` males are not included in the army conversion.
- `military` does not multiply displayed army size, and technology defense
  bonus remains battle-resolution-only.
- Vassal callable soldiers and mercenaries are not included in the Country
  Population Army split.
- War casualties deduct real population from male/female 25-64 cohorts using
  the same contribution weights.
- The tiny-cohort aging remainder is runtime-only and is reset on city init,
  new-world/clear-world paths, and post-load handling; save format is unchanged.
- Technology monthly progress still uses the existing effective-disorder
  percentage and remainder path.

## Validation

Focused implementation evidence from the Ver0.3.3 work:

- `make -B world_sim.exe`: passed before final documentation-only commit work.
- `cmd /c build.bat`: passed before final documentation-only commit work.
- `make check-text`: passed.
- `git diff --check`: passed with line-ending warnings only.
- No `.c` includes another `.c`.
- Touched and new `.c/.h` files are at or below 500 lines.
- Focused `--probe-population` evidence covered army formula, weighted
  casualties, small-population fractional diagnostics, tiny-cohort aging,
  aging-remainder reset, single-civilization auto-run behavior, and technology
  duration formulas.
- English and Chinese Country Population screenshots showed the new two-row
  card layout, male/female splits for the second row, one visible `75+` row,
  and no visible `81+` row.

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. A live `world_sim.exe` process owned by the user was present during
the release attempt, and the user explicitly instructed the release agent not to
manage that executable and to push after finishing documentation. Therefore this
commit records focused evidence and version metadata, but it must not be treated
as full Rule39 release-readiness evidence.

## Residual Risks

- The slower technology cadence makes Rule39 and long-run balance validation
  materially longer.
- Population balance remains a watch item because stronger old-age mortality,
  tiny-cohort aging, army conversion, and resource/capacity pressure interact
  over multi-century runs.
