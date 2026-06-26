# Ver0.3.4.b Side Doc

Ver0.3.4.b is a checkpoint over Ver0.3.4.a for early-expansion static map
border-cache performance and border visual correctness.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.4.b`.
2. Added `--probe-expansion-perf` to measure Extreme-map expansion-claim static
   cache behavior.
3. Added incremental static border overlay updates so small ownership/province
   changes can avoid broad full-overlay border rebuilds.
4. Fixed the zoomed-border visual regression by clearing and redrawing the full
   affected neighborhood, then verifying incremental output against full border
   rebuild output.
5. Preserved country borders as the darker, thicker border type.
6. Changed province borders to the warmer brown `RGB(104, 76, 46)` while
   preserving province-border semantics and width.
7. Preserved dotted map-grid rendering.
8. Ignored generated expansion performance logs and zoom BMP artifacts so they
   remain local validation evidence instead of source-controlled files.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/version.h`
- `src/game/game.h`
- `src/game/game_expansion_perf_probe.c`
- `src/main.c`
- `src/render/render_static_map_cache.c`
- `src/render/render_static_map_cache_border.c`
- `.gitignore`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.4.b_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `15`.
- No expansion gameplay rules changed.
- No ownership or political fill semantics changed.
- No city icon, diplomacy, war, alliance, name-pool, world-generation, save
  format, population, plague, economy, or balance rule changes are intended.
- Country borders are still based on alive land-owner differences.
- Province borders are still based on same alive owner plus different valid
  `province_id`.
- The map grid remains dotted by design.

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
- `world_sim_validation_tmp.exe --probe-expansion-perf` passed with
  `overall_ok=1`.
- `world_sim_validation_tmp.exe --probe-presentation` passed with
  `overall_ok=1`.
- `world_sim_validation_tmp.exe --probe-diplomacy` passed with `overall_ok=1`.
- `world_sim_validation_tmp.exe --probe-worldgen` passed with `overall_ok=1`.
- `--probe-expansion-perf` records Extreme-map static cache timing and border
  exactness evidence in local ignored `logs/` artifacts.
- The focused border probe checked:
  - incremental border hash equals full rebuild hash,
  - province border color `RGB(104, 76, 46)` appears,
  - country border continuity,
  - province border continuity,
  - dotted grid preservation.
- The generated local zoom artifact was `logs/expansion_border_zoom.bmp`.
- Full AGENTS Rule39 validation must still be completed before claiming broad
  release-ready/gameplay acceptance for this checkpoint.

## Residual Risks

- Focused/offscreen probes prove the targeted border cache path and visual
  exactness, but live long-run GUI performance still needs full Rule39 evidence
  before release-ready claims.
- Early-expansion stutter may still have other contributors outside the static
  border overlay path, such as snapshot publish cadence, city overlays, labels,
  or panel invalidation.
