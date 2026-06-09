# Ver0.3.2.f Side Doc

Ver0.3.2.f is a focused population formula and World Population presentation
checkpoint over Ver0.3.2.e. It accepts the requested pressure-to-birth and
pressure-death curve changes while keeping the Ver0.3.2.e economy, map-cache,
highlight, render-performance, treasury, and war-economy baseline intact.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.f`.
2. Updated the birth multiplier curve to:
   - pressure 0 = 105%
   - pressure 10 = 98%
   - pressure 25 = 90%
   - pressure 45 = 50%
   - pressure 65 = 35%
   - pressure 85 = 12%
   - pressure 100 = 3%
3. Updated pressure-death estimation to use a piecewise deaths-per-million
   curve:
   - pressure below 25 = 0 deaths/month
   - pressure 25 = 280 deaths per million/month
   - pressure 40 = 480 deaths per million/month
   - pressure 50 = 1500 deaths per million/month
   - pressure 65 = 3500 deaths per million/month
   - pressure 85 = 8000 deaths per million/month
   - pressure 100 = 30000 deaths per million/month
4. Preserved the existing effective-fertility model:
   `18-24 female * 100% + 25-39 female * 75% + 40-54 female * 8%`, capped by
   reproductive male population.
5. Preserved the existing monthly birth divisor of 9000.
6. Preserved pressure-death distribution as proportional across age cohorts,
   while natural age deaths remain old-age biased and child accidental deaths
   remain limited to the 0-4 cohort.
7. Updated the World Population pyramid to use age-normalized density, matching
   the country Population tab semantics.
8. Split World Population pressure display into global carrying usage, average
   country carrying pressure, and high-pressure country count.

## Files In Scope

- `src/core/version.h`
- `src/sim/population_diagnostics.c`
- `src/render/panel_population.c`
- `src/render/panel_population_page.c`
- `src/render/render_panel_internal.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.f_side_doc.md`

## Behavioral Notes

- `Actual Population Pressure` remains the maximum of national resource
  pressure and local capacity-overload pressure.
- The release changes the response curve once a civilization is under pressure;
  it does not add a new death category or alter treasury, economy, expansion,
  war, plague, route, map-cache, highlight, or diplomacy systems.
- World Population now distinguishes global aggregate carrying usage from the
  average of country-level carrying pressure, so a world with spare aggregate
  capacity can still show that many individual countries are crowded.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.f`.
- Focused formula probes reported exact birth multiplier nodes.
- Focused pressure-death probes on 1,000,000 population reported 0, 0, 280,
  480, 1500, 3500, 8000, and 30000 deaths/month at the requested pressure
  points.
- Focused scale probes reported about 5023 pressure deaths/month for 4.6M
  population at pressure 46, and about 1878 pressure deaths/month for 370K
  population at pressure 72.
- Distribution checks confirmed pressure deaths remain proportional across all
  age cohorts, natural deaths remain old-age biased, and child accidental deaths
  remain limited to the 0-4 cohort.
- GUI evidence covered World Population in English and Chinese plus low- and
  high-pressure country Population views.
- Three Large-map, 26-civilization, >600-region, 240-year tuning probes ended
  with population/capacity ratios of 99.14%, 93.94%, and 91.78%.
- Full Rule39 validation used a Large map with 26 generated civilizations, 723
  natural regions, max/5x speed, reached Year 429 Month 7, and ended with
  35 civilization slots / 31 alive.
- Rule39 stage-5 examples were `1 Western LuoLong Protectorate`, `2 Realm of
  Veyr`, `3 LinMing Banner State`, `4 High Kingdom of Solmere`, and `6
  Redmere Dominion`.
- Rule39 deep-sea evidence reported official counters changing from 0 total /
  0 shallow / 0 deep before unlock to 49 total / 48 shallow / 1 deep after
  unlock.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 21228.
- A temporary-target build with `TARGET=tmp_worldsim_ver032f_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.f`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` and `git diff --check` passed, with only Git CRLF warnings.
- Static checks found no `.c` file includes another `.c`.
- All touched source/header files are at or below 500 lines.

## Residual Risks

- Multi-seed tuning probes ended with negative average monthly net growth, so
  this curve should be watched in later balance passes.
- Rule39 final throughput was good at 11.63 months/sec with queue 0, but the
  final snapshot still recorded a 235 ms frame peak. This remains a watch item
  rather than a blocker for this focused population checkpoint.
