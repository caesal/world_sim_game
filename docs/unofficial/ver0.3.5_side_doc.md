# Ver0.3.5 Side Doc

Ver0.3.5 is a Military Alliance and Alliance Council checkpoint over
Ver0.3.4.b.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5`.
2. Added Military Alliances as an evolved alliance type after long-lived,
   high-relation defensive alliances pass an automatic leader-initiated vote.
3. Added Alliance Council vote weights with 1000 internal units and 80 displayed
   council seats.
4. Added 8-year council elections, with immediate recalculation after member
   joins or exits.
5. Added weighted Military Alliance upgrade voting, stricter Military Alliance
   join voting, and updated alliance removal/join timing constants.
6. Added Military Alliance war support, with non-primary members contributing
   real reinforcement pools and sharing casualties/disorder consequences.
7. Disabled the defensive-alliance war-defeat exit benefit for Military
   Alliances, while preserving ordinary alliance exit and removal behavior.
8. Reduced Military Alliance union eligibility from 800 to 500 years while
   keeping Defensive Alliance union eligibility at 800 years.
9. Added Alliance Council UI rendering with 80 visible seats, a compact council
   table, threshold chips, election timing, and aligned central dais spacing.
10. Added vote-year council snapshots so historical Military Alliance votes
    remain self-consistent after council elections or membership changes.
11. Fixed candidate join vote display so the candidate does not vote in its own
    join vote and passed Military Alliance join votes cannot display below the
    61/80 threshold.
12. Updated build lists and probe routing for Military Alliance validation.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/render_snapshot.h`
- `src/core/render_snapshot_cache.c`
- `src/core/version.h`
- `src/game/game.h`
- `src/game/game_alliance_probe.c`
- `src/game/game_diplomacy_probe.c`
- `src/game/game_military_alliance_probe.c`
- `src/game/game_military_alliance_rules_probe.c`
- `src/io/map_save.c`
- `src/io/map_save_state.c`
- `src/main.c`
- `src/render/panel_alliance.c`
- `src/render/panel_alliance_council.c`
- `src/render/panel_alliance_council.h`
- `src/render/panel_alliance_detail.c`
- `src/render/panel_alliance_history.c`
- `src/render/panel_alliance_model.c`
- `src/render/panel_alliance_model.h`
- `src/render/panel_alliance_vote_state.c`
- `src/render/panel_alliance_votes.c`
- `src/render/panel_country_diplomacy_cards.c`
- `src/render/panel_diplomacy_cache_key.c`
- `src/sim/alliance.h`
- `src/sim/alliance_ai.c`
- `src/sim/alliance_council.c`
- `src/sim/alliance_military.c`
- `src/sim/alliance_military.h`
- `src/sim/alliance_query.c`
- `src/sim/alliance_records.c`
- `src/sim/alliance_state.c`
- `src/sim/alliance_union.c`
- `src/sim/war.c`
- `src/sim/war.h`
- `src/sim/war_names.c`
- `src/sim/war_resolution.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` is now `17` because alliance save state now includes
  Military Alliance council data and vote-year council snapshots.
- Defensive Alliance join voting remains all-member approval.
- Military Alliance join voting uses weighted council votes and requires strict
  approval above the 3/4 threshold, shown as `61 / 80`.
- Military Alliance upgrade voting uses weighted council votes and requires
  strict approval above the 2/3 threshold, shown as `54 / 80`.
- Normal candidate timing uses 30 years for the first vote and 10 years for
  retry votes.
- Military Alliance candidate support probability is stricter than Defensive
  Alliance support and is relation-driven from 80 to 100 relation.
- The candidate country does not receive votes in its own join vote.
- Historical vote records use vote-year council allocation rather than current
  council allocation.
- No map ownership/fill, world-generation, route-potential, plague, population,
  or economy rules are intentionally changed by this checkpoint.

## Validation Notes

- Canonical `make -B world_sim.exe` passed.
- Canonical `cmd /c build.bat` passed.
- `git diff --check` passed with line-ending warnings only.
- `make check-text` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched/new `.c/.h` line counts were at or under the 500-line limit.
- `world_sim.exe --probe-military-alliance` passed with `probe=military_alliance ok=1`.
- The Military Alliance probe reported:
  - `case=military_join_vote_snapshot ok=1`
  - `yes=62`
  - `no=18`
  - `candidate_seats=0`
  - `fail_yes=60`
  - `fail_passed=0`
- `world_sim.exe --probe-presentation` passed with `overall_ok=1`.
- `world_sim.exe --probe-diplomacy` passed with `overall_ok=1`.
- `world_sim.exe --probe-worldgen` passed with `overall_ok=1`.
- The latest worldgen probe used an Extreme `1152x800` map with `26`
  civilizations and `1091` natural regions.
- Full AGENTS Rule39 validation was not completed for Ver0.3.5. This checkpoint
  must not be treated as full release-ready gameplay acceptance until a fresh
  Rule39 run records final year/month, natural region count, civilization count,
  speed setting, five technology-stage-5 civilizations, and deep-sea
  hidden-before/revealed-after evidence.

## Residual Risks

- Focused probes cover the Military Alliance vote rules, vote-year snapshot
  display, council math, council rendering artifact generation, diplomacy,
  presentation, and worldgen paths, but do not replace a live long-run Rule39
  regression.
- Live GUI inspection of naturally occurring Military Alliance votes and union
  outcomes remains useful before broad release-ready claims.
