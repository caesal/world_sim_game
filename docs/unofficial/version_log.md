# Version Log

## Ver0.3.3.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.3.a.
2. Added an Extreme map size at `1152x800` while preserving the existing Small,
   Medium, and Large choices.
3. Added map-size-specific natural-region caps:
   Small `512`, Medium `768`, Large `1024`, Extreme `1536`.
4. Updated world-generation estimates and generation repair/headroom behavior
   so they clamp to the active map's region cap instead of always using the
   global maximum.
5. Updated the world-generation panel and hit layout for four map-size buttons,
   including the localized Extreme label.
6. Changed the Physical random button so each physical slider is randomized
   independently in the `5..95` range.
7. Changed the Advanced random button so forest, desert, mountain, wetland, and
   natural-region-size sliders are randomized independently in the `5..95`
   range.
8. Added an Extreme-map presentation policy that raises province borders to
   `2px` and country borders to `3px` across static cache, snapshot fallback,
   contour, and vector border paths.
9. Reduced Extreme-map city and port marker sizes while keeping capital rings
   and harbor glyphs recognizable.
10. Smoothed the display-only population pyramid band from ages `65..74`,
    preserving male, female, and total population values exactly.
11. Updated the in-game pause-menu version summary, including a cleaned UTF-8
    Chinese release-note block.

Validation notes:

- Ver0.3.3.a uses `WORLD_SIM_VERSION "0.3.3.a"`.
- Focused map-size probe evidence is recorded at
  `build/validation/map_size_extreme_20260612/map_size_probe_output.txt`.
  It generated Small `576x400`, Medium `720x500`, Large `864x600`, and Extreme
  `1152x800`, with Extreme producing `1093/1536` natural regions and
  `failures=0`.
- Focused population display evidence is recorded at
  `build/validation/population_display_65_74_20260612/summary.txt`. It
  preserved male `1000`, female `800`, and total `1800`, and smoothed
  `65-69` and `70-74` to `900/900` with `failures=0`.
- GUI evidence for Extreme-map readability and population display is recorded
  under `build/validation/extreme_visual_readability_20260612/gui/`.
- Strict AGENTS Rule39 game-flow regression was not completed for this pushed
  checkpoint. This version must not be treated as full release-readiness
  evidence until Rule39 is run and recorded.

## Ver0.3.3

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.3.
2. Added a population-derived army model using male 25-64 at 2.5% and female
   25-64 at 1%, with current soldiers subtracting active-war casualties.
3. Added weighted population casualty deduction from male/female 25-64 cohorts.
4. Added deterministic tiny-cohort aging remainder support and reset it across
   city init, clear-world/new-world, and post-load paths.
5. Kept real population storage on the existing 8 `PopulationCohort` bands and
   kept save format unchanged.
6. Added fractional x100 birth, total-death, and net-change diagnostics for
   small populations.
7. Updated Country Population cards to show `Children / Fertile / Elder` over
   `Workers / Recruitable / Army`, with the second row split by male/female.
8. Removed the duplicated `Usage` card from the structure-card grid while
   keeping the top carrying-usage overview.
9. Changed one-civilization auto-run behavior so auto-run stops only when the
   living civilization count is zero.
10. Updated technology stage timing to a 120-year innovation-5 baseline, 5 years
    per innovation point, larger resource/pressure modifiers, late-stage 92%
    duration scaling, and an 80..150 year clamp.

Validation notes:

- Ver0.3.3 uses `WORLD_SIM_VERSION "0.3.3"`.
- Final release validation evidence is recorded in
  `docs/unofficial/ver0.3.3_side_doc.md`.
- Strict Rule39 was not completed before this push; the side doc records the
  focused evidence and the explicit limitation.

## Ver0.3.2.g

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.g.
2. Added a display-only yearly population cohort cache so country and world
   population pyramids can show cohort waves moving upward over time without
   changing the real 8-band gameplay population storage.
3. Kept the visible population pyramid row structure compact: the UI still
   shows one `75+` row and does not expose an `81+` row.
4. Updated pyramid bar scaling to use density-style values: 5-year rows divide
   by 5, while the visible `75+` row divides by 12 so it no longer visually
   dominates only because it covers more years.
5. Replaced old-age natural mortality with the requested monthly rates:
   55-64 = n / 200, 65-74 = n / 83, 75-80 = n / 24, and 81+ = n * 8 / 100.
6. Used display cohorts only to split the real `75+` gameplay bucket into
   75-80 and 81+ for mortality calculation; actual real deductions still come
   from the existing real `75+` bucket.
7. Added a focused population probe entrypoint and mortality/display helpers
   for formula, split, wave, display-scale, and multi-seed balance validation.

Validation notes:

- Ver0.3.2.g uses `WORLD_SIM_VERSION "0.3.2.g"`.
- Focused formula probes reported monthly natural deaths of 5000, 12048,
  41667, and 80000 for 1,000,000 people in 55-64, 65-74, 75-80, and 81+.
- The split probe reported 60K people in 75-80 plus 40K in 81+ removed 5700
  people from the real `75+` bucket while keeping the UI row label as `75+`.
- Display-wave probes reported a birth batch reaching `5-9` after 6 years and
  `20-24` after 20 years while preserving the old real 8-band aging behavior.
- Focused GUI evidence covered country Population in English and Chinese,
  elderly fixture views, world Population in English and Chinese, and the
  Population-tab Debug / Performance sample.
- Multi-seed Large-map, 26-civilization, >600-region probes to Year 240 ended
  with population/capacity ratios of 98%, 100%, and 90%; two seeds produced one
  near-extinct civilization each, so longer balance validation remains a watch
  item.
- Population-tab max-speed focused evidence reported 93 ms/month,
  10.75 months/sec, queue 1, coalesced 0, frame 76/188, and render 63/125.
- Strict AGENTS Rule39 was not rerun for this checkpoint; this release is based
  on focused probes plus user acceptance after manual inspection.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 29828.
- A temporary-target build with `TARGET=tmp_worldsim_ver032g_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.g`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` and `git diff --check` passed.
- Static checks found no `.c` file includes another `.c`, and all touched `.c`
  / `.h` files are at or below 500 lines.

## Ver0.3.2.f

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.f.
2. Updated the population birth multiplier curve to the requested pressure
   nodes: 0=105%, 10=98%, 25=90%, 45=50%, 65=35%, 85=12%, and 100=3%.
3. Replaced the pressure-death estimate with a piecewise deaths-per-million
   curve: pressure below 25 produces 0 pressure deaths, then 25=280, 40=480,
   50=1500, 65=3500, 85=8000, and 100=30000 deaths per million people per
   month.
4. Preserved the existing weighted-fertility model, effective-pair cap,
   9000 monthly birth divisor, proportional pressure-death distribution,
   old-age natural mortality, and child accidental mortality semantics.
5. Made the World Population pyramid use the same age-normalized density
   semantics as country Population views, using display widths 5, 13, 7, 15,
   15, 10, 10, and 12 years for the existing age cohorts.
6. Split world-level population pressure presentation into global carrying
   usage, average country carrying pressure, and high-pressure country count.

Validation notes:

- Ver0.3.2.f uses `WORLD_SIM_VERSION "0.3.2.f"`.
- Focused formula probes reported exact birth multiplier nodes and pressure
  death nodes, including 1M-population pressure-death outputs of 0, 0, 280,
  480, 1500, 3500, 8000, and 30000 for the requested pressure points.
- Focused scale probes reported about 5023 pressure deaths per month for
  4.6M people at pressure 46, and about 1878 pressure deaths per month for
  370K people at pressure 72.
- Distribution checks confirmed pressure deaths remain proportional across
  age cohorts, natural deaths remain old-age biased, and child accidental
  deaths remain limited to the 0-4 cohort.
- GUI evidence covered World Population in English and Chinese plus low- and
  high-pressure country Population views.
- Three Large-map, 26-civilization, >600-region, 240-year tuning probes ended
  with population/capacity ratios of 99.14%, 93.94%, and 91.78%; final average
  monthly net growth was negative in all three probes, so the curve remains a
  watch item for future balance passes.
- Rule39 validation used a Large map with 26 generated civilizations, 723
  natural regions, max/5x speed, reached Year 429 Month 7, and ended with
  35 civilization slots / 31 alive.
- Rule39 stage-5 examples were `1 Western LuoLong Protectorate`, `2 Realm of
  Veyr`, `3 LinMing Banner State`, `4 High Kingdom of Solmere`, and `6
  Redmere Dominion`.
- Rule39 deep-sea evidence reported official counters changing from 0 total /
  0 shallow / 0 deep before unlock to 49 total / 48 shallow / 1 deep after
  unlock.
- Rule39 final throughput was 11.63 months/sec with queue 0, but the final
  snapshot recorded a 235 ms frame peak; this remains a performance watch item.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 21228.
- A temporary-target build with `TARGET=tmp_worldsim_ver032f_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.f`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` and `git diff --check` passed, with only Git CRLF
  warnings.
- Static checks found no `.c` file includes another `.c`, and all touched `.c`
  / `.h` files are at or below 500 lines.

## Ver0.3.2.e

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.e.
2. Added persistent capped treasury state, annual economy settlement, resource
   pressure, deficit buffering, stability spending, war indemnity offsets, and
   temporary mercenary support hooks.
3. Preserved commerce as trade capability and money as territorial money
   potential while adding treasury as a separate national reserve.
4. Added save compatibility plumbing for expanded civilization state.
5. Updated population diagnostics and Population tab presentation with effective
   pressure, weighted fertility, proportional pressure deaths, small-population
   natural mortality, Top 6 city rows, city display names, type/status columns,
   and compact monthly-change/treasury cards.
6. Updated decision/war desire diagnostics so resource crisis with no reachable
   expansion target can raise war pressure while existing hard gates still
   suppress invalid declarations.
7. Added runtime-only profiling switches and expanded Debug / Performance rows
   for side-panel detail draw, highlight, city overlay, map labels, static
   scene/cache, diplomacy animation, map legend, panel cache rebuild, treasury,
   resource pressure, and political publish latency.
8. Batched selected-country highlight work, added contour/edge caching, and
   reduced highlight hot-path GDI churn.
9. Split static map and static scene responsibilities so stale safe frames no
   longer hide current ownership colors.
10. Fixed Political/All/Regions ownership-color latency by allowing current
    political fill and borders to publish before full coast/hydro/static compose
    completion.

Validation notes:

- Ver0.3.2.e uses `WORLD_SIM_VERSION "0.3.2.e"`.
- Focused ownership-latency evidence reproduced pre-fix political publish lag
  at 143250 ms and reduced focused post-fix max pending latency to 78 ms.
- Rule39 ownership validation reported max political pending latency of 31 ms.
- Selected-detail matrix validation stayed around 9.17-11.36 months/sec without
  repeated 235-250 ms selected-detail peaks.
- Full Rule39 validation used a Large map with 26 initial civilizations, 712
  natural regions, max/5x speed, reached Year 435 Month 6, and reported 24
  stage-5 civilizations plus 6 stage-6 civilizations.
- First five stage-5 civilizations in the Rule39 run were `0 Thornwatch
  Kingdom`, `1 TengriAkane Jade Dominion`, `2 Great Qinghe Dynasty`, `3
  Goldenreach League`, and `4 MoriChen River League`.
- Deep-sea evidence reported visible deep routes transitioning from 0 before
  unlock to 2 after unlock.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 48996.
- A temporary-target build with `TARGET=tmp_worldsim_ver032e_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.e`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`, and all touched `.c`
  / `.h` files are at or below 500 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- Rule39 recorded isolated late-run frame peaks up to 250 ms during late
  technology/deep-sea transition; throughput remained near 10 months/sec, so
  this is tracked as a performance watch item.

## Ver0.3.2.d

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.d.
2. Made selected-overlord vassal relation highlights pulse direct vassal focus
   points immediately, matching the already-working selected-vassal behavior.
3. Applied focused Phase 5 clay presentation polish to Population, Plague,
   Map/Info, Debug / Performance, and shared progress/metric widgets while
   preserving dense diagnostic rows and existing visible content.
4. Removed the `render_common -> ui_clay_widgets` dependency introduced during
   Phase 5 work by moving Info metric clay rendering into the concrete Info
   panel path.
5. Centralized lighter clay soft-shadow tokens with reduced offsets so controls
   keep more visual body and rely less on heavy shadows.
6. Added active Plague presentation evidence in English and Chinese, in addition
   to the no-active-outbreak state.
7. Fixed diplomacy/event map animations so lines, glow, arrowheads, and markers
   clip to the actual map viewport/map rectangle.
8. Stored and validated snapshot map dimensions for queued diplomacy map
   animations, skipping stale or invalid endpoints instead of remapping old
   coordinates through current global map dimensions.
9. Reduced long-distance diplomacy animation bowing so orange/gold event arcs do
   not look like large route lines crossing outside the map.
10. Changed war-start map animation to a single red arrow from attacker to
    defender instead of a red bidirectional arrow.

Validation notes:

- Ver0.3.2.d uses `WORLD_SIM_VERSION "0.3.2.d"`.
- Canonical `make -B world_sim.exe` succeeded.
- `cmd /c build.bat` succeeded.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`, and all touched
  `.c` / `.h` files are at or below 500 lines.
- The repo-wide line-count scan still reports the pre-existing untouched
  `src/sim/plague.c` at 515 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- Focused UIUX evidence covered lighter shadows, Population English/Chinese,
  Plague no-active and active states in English/Chinese, route-potential
  route-only legend, Debug Map & Log ampersand rendering, Debug / Performance
  lower rows, selected lower-edge highlight, pause menu, speed buttons, and
  Phase 4 diplomacy card preservation.
- Focused diplomacy animation evidence covered long orange animation clipping,
  red war-start attacker-to-defender direction, political map sanity, and
  route-potential route-only legend sanity.
- Full strict AGENTS Large-map regression was not rerun for this focused
  render/UIUX checkpoint.

## Ver0.3.2.c

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.c.
2. Fixed peace-pressure war settlement so attacker and defender roles determine
   the outcome: defender-only willingness becomes attacker victory/surrender,
   attacker-only willingness becomes an offensive halt, and both-willing
   willingness becomes a no-winner negotiated truce.
3. Preserved the 25-year truce cooldown for offensive halts, severed-front
   interruptions, and negotiated truces, while decisive cession outcomes keep
   the 55-year truce cooldown.
4. Added visible last-war result kinds for military win/loss, surrender,
   negotiated truce, offensive halt, and severed-front interruption.
5. Added defeated-country vassalization when no region is actually transferred,
   or when post-settlement disorder/cohesion meet the severe-instability
   thresholds.
6. Deducted one cohesion when a civilization loses its capital.
7. Changed collapse and enclave successor cohesion to parent cohesion plus a
   bounded 1-3 random bonus, capped at 10.
8. Applied Claymorphism Phase 4 presentation to Country list cards, selected
   summaries, action buttons, overview metric chips, diplomacy tabs, diplomacy
   cards, semantic relation accents, truce spacing, and vassal hierarchy rows.
9. Updated active-war cards to show country names above troop numbers,
   attacker/defender roles beneath troop numbers, existing peace-pressure bars,
   casualties and wins on the first metric row, and front/disorder on the
   second row.
10. Sorted the War & Truce list with active wars first, then truces by remaining
    years descending.
11. Ignored the local `logs/uiux_phase4_evidence/` validation screenshot folder
    so release commits do not include large local evidence artifacts.

Validation notes:

- Ver0.3.2.c uses `WORLD_SIM_VERSION "0.3.2.c"`.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by the operating system.
- A temporary-target build with `TARGET=tmp_worldsim_ver032c_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.c`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- Static and text checks were rerun before push, and all scanned source/header
  files were at or below 500 lines.
- Focused validation evidence from the Software Engineer and UIUX passes
  covered the war peace-pressure probe, active-war diplomacy card render probe,
  Country overview metric grid, route-potential route-only legend, real
  diplomacy cards, truce sorting, vassal hierarchy rows, speed buttons,
  generated political map, world-generation progress overlay, and Debug /
  Performance readability.
- Full strict AGENTS Large-map regression was not rerun for this mixed
  gameplay/UIUX checkpoint.

## Ver0.3.2.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.b.
2. Revalidated the existing Phase 1, Phase 2A, and Phase 2B clay presentation
   baseline before applying Phase 3.
3. Added reusable clay widget helpers for section headers, input frames,
   sliders, and color swatches.
4. Applied clay presentation styling to World Setup section headers, map-size
   buttons, random buttons, slider tracks and knobs, input frames, civilization
   color previews, and color swatches.
5. Applied clay panel/button styling to the color picker and added narrow
   hover/pressed state handling for Auto, Apply, and Cancel.
6. Preserved native Win32 edit fields and Add/Apply child buttons to avoid
   changing text-input and focus behavior.
7. Preserved world-generation rules, simulation behavior, map-display rules,
   vassal behavior, route behavior, diplomacy, plague, population, economy,
   technology, saves, and balance constants.

Validation notes:

- Ver0.3.2.b uses `WORLD_SIM_VERSION "0.3.2.b"`.
- Canonical `make -B world_sim.exe` succeeded.
- `cmd /c build.bat` succeeded.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`, and all `.c` / `.h`
  files are at or below 500 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- String checks found `World Sim Game Ver 0.3.2.b` in `world_sim.exe`.
- Focused UI/UX evidence covered World Setup controls, color picker
  hover/pressed states, active world-generation progress overlay, generated
  political colors after generation, route-potential route-only legend, native
  child-control lifecycle, side-panel scrolling, pause menu, and Debug /
  Performance readability.
- The user approved the focused Phase 3 UI/UX checkpoint for push.
- Full strict AGENTS Large-map regression was not rerun for this focused UI/UX
  presentation checkpoint.

## Ver0.3.2.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.a.
2. Preserved the current run state when clicking direct-vassal Release and
   Annex actions.
3. Preserved the current run state when clicking the selected-country
   Independence action, which uses the same vassal release request path.
4. Made right-side Country, World, and Debug panel mouse-wheel scrolling handle
   wheel input immediately instead of waiting behind map-zoom batching.
5. Limited the country-list toggle to two exclusive views: active countries
   only, or fallen countries only.
6. Changed the country-list toggle label so the fallen-only view offers
   `Show Active Countries` and the active-only view offers
   `Show Fallen Countries`.
7. Clear the selected country when it no longer matches the active/fallen list
   filter.

Validation notes:

- Ver0.3.2.a uses `WORLD_SIM_VERSION "0.3.2.a"`.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by the operating system.
- A temporary-target build with `TARGET=tmp_worldsim_ver032a_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.a`,
  `Show Active Countries`, and `Show Fallen Countries`, and the temporary
  executable was deleted.
- `build.bat` was attempted and reached the link step, but was blocked by the
  same locked canonical executable.
- Static and text checks were rerun before push; touched source/header files
  were at or below 500 lines, while the pre-existing unrelated
  `src/sim/plague.c` remains 515 lines.
- Full strict AGENTS Large-map regression was not rerun for this hotfix
  checkpoint.

## Ver0.3.2

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.2.
2. Added `src/game/game_vassal_actions.c` and included it in both build lists.
3. Moved vassal release request handling out of `src/game/game.c` and added a
   direct-vassal annex request path.
4. Added compact direct-vassal overview rows with colored clickable vassal name
   cells plus Release and Annex actions.
5. Made vassal name-cell clicks select and locate the vassal without entering
   the modal/action pause path.
6. Updated ordinary city marker thresholds to Outpost below 800, Village from
   800-5399, Town from 5400-9499, and City from 9500 upward while keeping
   capital override first.
7. Kept city and harbor markers lightweight GDI drawings while slightly
   thickening harbor strokes.
8. Added a political-map legend glyph column with icon + name only for Outpost,
   Village, Town, City, Capital, Harbor, and Harbor Capital.
9. Kept route-potential legend output route-only with shallow and deep route
   entries.

Validation notes:

- Ver0.3.2 uses `WORLD_SIM_VERSION "0.3.2"`.
- Canonical `make -B world_sim.exe` and `build.bat` were attempted before push;
  both reached the link step and were blocked by a locked `world_sim.exe`.
- A temporary-target release verification build succeeded and the temporary
  executable was deleted.
- Static and text checks were rerun before push.
- Focused GUI evidence covered the political legend glyph column, route-only
  route-potential legend, marker zoom display, and legend collapse/expand.
- Focused vassal probes covered row hit targets, bright/dark name-cell contrast,
  Release, and Annex command behavior.
- The user confirmed the focused-only validation and approved this checkpoint
  for push.
- Full strict AGENTS Large-map regression was not rerun for this focused
  vassal/UI/map-display checkpoint.

## Ver0.3.1.c

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.1.c.
2. Added `src/sim/collapse_partition.c/.h` and included it in both build
   lists.
3. Replaced the old multi-province collapse behavior that scanned natural
   regions by id and capped each successor at 6 neighboring regions.
4. Added owned-region successor count bands: 1/2/3/4/5 successors for
   2-35 / 36-72 / 73-128 / 129-172 / >172 owned regions, with slot shortage
   reducing the requested successor count safely.
5. Added capital-core retention, farthest-first successor seeding, balanced
   multi-source region assignment, and internal successor-capital selection.
6. Kept single-province collapse behavior unchanged.

Validation notes:

- Ver0.3.1.c uses `WORLD_SIM_VERSION "0.3.1.c"`.
- Build/static validation was reported passing for the implementation and was
  rerun before push.
- Focused probes reported that single-province collapse was unchanged and that
  20/50/100/150/180+ region partition cases produced expected successor counts
  and connected balanced blocks.
- The implementation agent's GUI automation was stopped by the user, so full
  automated GUI validation and strict AGENTS regression were not completed by
  that agent.
- The user manually validated the executable result and approved this
  checkpoint for push.

## Ver0.3.1.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.1.b.
2. Batched large-country highlight fill into a single transparent overlay blend
   per pass, avoiding per-tile DIB/DC churn during country selection.
3. Populated Civil Unrest snapshot button state from collapse blockers and made
   the Civil Unrest command preserve the current pause/run state.
4. Added a manual Civil Unrest exception to collapse grace: grace still blocks
   pressure/natural collapse, but the player action may collapse again while
   grace remains active.
5. Changed active war battle cadence from 3 years / 36 months to 2 years / 24
   months and aligned diplomacy and decision-panel battle countdown displays.
6. Kept Release Vassal behavior unchanged after audit and user validation.

Validation notes:

- Ver0.3.1.b uses `WORLD_SIM_VERSION "0.3.1.b"`.
- Build/static validation was reported passing for the implementation, and the
  release build/static checks were rerun before push.
- Focused GUI validation used a Large map with randomized physical and advanced
  terrain settings and 5 civilizations, per the focused exception for this
  follow-up. It verified large-country highlight responsiveness, Civil Unrest
  without forced pause, repeated manual Civil Unrest during collapse grace, and
  24-month battle countdown alignment.
- Full 26-civilization strict game-flow regression was not rerun for this
  focused interaction checkpoint.

## Ver0.3.1.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.1.a.
2. Added a persistent 432-month hard reinfection cooldown for cities after
   plague recovery.
3. Centralized the cooldown gate through `plague_seed_city()` so direct seeding,
   random outbreak selection, local spread, maritime spread, migration spread,
   and war-casualty plague seeding cannot reinfect a cooling city.
4. Bumped `MAP_SAVE_VERSION` from 10 to 11 and added v10 `PLGC` plague-state
   conversion with new city cooldown initialized to 0.
5. Tightened max-speed overloaded presentation coalescing so large-map
   rendering pressure does not stall UI interaction as aggressively.

Known follow-up:

- The high-load presentation coalescing is intentionally visual-only and should
  continue to be monitored for stale map color, route, city-marker, and plague
  overlay delays on Large maps.
- Future plague tuning should distinguish random-outbreak immunity, city
  reinfection cooldown, and plague-pressure accumulation in reports.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.3.1.a uses `WORLD_SIM_VERSION "0.3.1.a"`.
- `MAP_SAVE_VERSION` is 11 because the `PLGC` dynamic save block now includes
  city reinfection cooldown state.
- Build/static validation was reported passing for the plague cooldown and
  presentation coalescing implementation.
- Focused probes reported that active plague can extend, recovery sets cooldown
  432, direct/local/maritime/migration/war reinfection paths are blocked,
  cooldown decrements, infection works again at 0, new saves preserve cooldown,
  and legacy v10 saves load cooldown as 0.
- The user manually validated the resulting executable experience and approved
  this checkpoint for push.

## Ver0.3.1

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.1.
2. Added weighted enclave-resolution outcomes when disconnected components
   reach the 360-month separation threshold: independent country,
   original-owner vassal, strongest land-neighbor integration, or unowned
   collapse.
3. Added fallback handling for full country slots, missing land neighbors, and
   failed claims while keeping `MAX_CIVS` at 200.
4. Capped collapse successor creation by owned natural-region count: countries
   with up to 35 owned regions create at most one successor, and larger
   countries create at most two.
5. Reset successful collapse total disorder and carry for the original country
   and successors while preserving underlying plague, war, resource, migration,
   and stability pressure sources.
6. Added collapse grace protection so immediate recollapse does not fire during
   the grace period.
7. Added fragmentation diagnostics for enclave outcomes, collapse outcomes,
   vassal break events, alive/independent/vassal counts, one-province counts,
   and slot usage.
8. Preserved the Ver0.3.0 marker, route-stability, and responsiveness work.

Known follow-up:

- Continue to monitor long-run country counts, especially collapse successor
  and enclave outcome rates on Large maps.
- Slot-full and no-land-neighbor fallback branches were not naturally hit in
  the reported long runs and should remain covered by focused probes.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.3.1 uses `WORLD_SIM_VERSION "0.3.1"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not intentionally
  change the binary save layout.
- Build/static validation was reported passing for the fragmentation-control
  pass.
- Focused probes reported the requested enclave weight tables:
  one-region pre-tech-6 `17/16/17/50`, one-region tech-6-plus `20/20/45/15`,
  multi-region pre-tech-6 `30/25/25/20`, and multi-region tech-6-plus
  `25/25/40/10`.
- Reported 20-civ Large run: 708 natural regions, final Year 236 Month 4,
  alive 77, independent 72, vassal 5, one-province 10, slots 77/200,
  enclave outcomes `I 11 / V 10 / J 8 / U 14`, and collapse successors 50.
- Reported 26-civ Large strict run: 698 natural regions, 26/26 placed,
  reached technology stage 5 by Year 366, final captured Year 383 Month 1,
  alive 106, independent 98, vassal 8, one-province 19, slots 112/200,
  enclave outcomes `I 21 / V 18 / J 27 / U 30`, and collapse successors 105.
- The reported strict run inspected map display, labels, routes, borders,
  diplomacy, war, vassals, collapse, plague, logs, and Debug / Performance
  diagnostics.

## Ver0.3.0

Implemented fixes:

1. Bumped the active prototype version to Ver0.3.0.
2. Restored recognizable lightweight city and settlement glyphs without
   returning map markers to the old per-marker PNG/GDI+ draw path.
3. Restored a distinct anchor-style harbor marker while preserving capital
   rings, neutral visibility rules, one-marker port behavior, and marker
   counters.
4. Stabilized sea-lane visual offset keys so route screen-side shifts no longer
   depend on `lanes_revision`, lane index, or route-potential edge index.
5. Derived route visual identity from stable render geometry inputs such as
   route type, regions, endpoint ports, map size, point count, and sampled path
   points.
6. Kept the Ver0.2.12.d stutter, year-jump, stale-snapshot, and UI click
   responsiveness improvements intact.

Known follow-up:

- Continue to watch marker and route rendering performance on dense Large maps.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.3.0 uses `WORLD_SIM_VERSION "0.3.0"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not change the binary
  save layout.
- Build/static validation passed for the render-only cleanup.
- Strict AGENTS regression was reported complete with a Large map, 26 initial
  civilizations, 748 natural regions, 748 cities, randomized map parameters,
  max speed, final Year 505 Month 1, five distinct civilizations at technology
  stage 5 or beyond, and deep-sea route hidden-before/revealed-after evidence.

## Ver0.2.12.d

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.12.d.
2. Recorded the current combined UI/UX Phase 2B, route-display, province-shape,
   render-cache, and max-speed responsiveness work as a recoverable checkpoint.
3. Preserved the successful stutter, year-jump, and UI click responsiveness
   improvements from the current dirty worktree before starting the next
   city/harbor marker visual follow-up.
4. Kept the new render layer cache source files in the build lists so the
   render cache split is part of the archived checkpoint.
5. Documented that city and harbor map markers currently use simplified
   lightweight glyphs; restoring recognizable marker visuals is the next
   intended follow-up and should not reintroduce per-marker GDI+ stutter.

Known follow-up:

- Restore recognizable city and harbor marker visuals using cached original
  icons or similarly lightweight recognizable vector/sprite rendering.
- The current checkpoint is intentionally archival: it prioritizes preserving
  the successful responsiveness state before the marker-icon follow-up.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.12.d uses `WORLD_SIM_VERSION "0.2.12.d"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not intentionally
  change the binary save layout.
- The user confirmed a major real-play improvement in stutter, year jumps, and
  UI click responsiveness after the performance pass.
- The performance pass reported strict AGENTS regression with a Large map, 26
  initial civilizations, 751 natural regions, randomized physical and advanced
  terrain settings, max speed, final Year 579 Month 7, five distinct
  civilizations at technology stage 5 or beyond, and deep-sea route
  hidden-before/revealed-after evidence.

## Ver0.2.12.c

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.12.c.
2. Removed the side-panel handle path's synchronous full-window `UpdateWindow`
   call so collapse/expand no longer forces an immediate heavy repaint.
3. Added a narrow side-panel immediate paint path for tab and subtab changes,
   letting side-panel navigation repaint without forcing a full map redraw.
4. Added cached-window blitting when full-map repaint work is requested while
   input is waiting, deferring the heavy repaint until the queue can process it
   without blocking user interaction.
5. Coalesced max-speed presentation redraws under render/simulation pressure
   without changing month progression, technology rules, war, population,
   world generation, or other gameplay semantics.

Known follow-up:

- The direct side-panel immediate paint path should remain centralized and
  should not be copied into unrelated UI controls without a broader partial
  paint API.
- Broader Phase 6 performance work may continue from this checkpoint if new
  bottlenecks are found, but future claims still require strict evidence.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.12.c uses `WORLD_SIM_VERSION "0.2.12.c"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not change the binary
  save layout.
- Build/static validation passed for the performance responsiveness scope.
- Strict AGENTS regression was reported complete with a Large map, 26 initial
  civilizations, 654 natural regions, randomized physical and advanced terrain
  settings, max speed, final Year 424 Month 11, five distinct civilizations at
  technology stage 5 or beyond, and deep-sea route hidden-before/revealed-after
  evidence.

## Ver0.2.12.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.12.b.
2. Added `src/ui/ui_clay_widgets.c` and `src/ui/ui_clay_widgets.h` for a
   reusable Claymorphism widget layer above the existing clay primitives.
3. Extended the clay primitive layer with pill and inset pill drawing so tiny
   floating icon buttons do not need an external square shadow.
4. Migrated the top-bar reset/language buttons, bottom play/speed buttons,
   side-panel handle, and pause-menu shell/buttons to the Phase 2A clay widget
   path while preserving existing hit tests, actions, labels, and panel
   content.
5. Fixed the collapsed side-panel handle artifact by bypassing the opaque
   collapsed mini-panel cache path and drawing the handle directly over the
   already-rendered scene.
6. Fixed `build.bat` so it mirrors the Makefile resource pipeline: it now
   builds `src/world_sim.rc` with `windres` and links
   `build/world_sim_resource.o`, preserving the application icon after both
   canonical build paths.
7. Added `world_sim.exe --no-activate` as a validation-only launch path using
   `SW_SHOWNOACTIVATE`, while keeping the normal launch path unchanged.

Known follow-up:

- Phase 6 large-map stutter and simulation-speed work remains outside this
  release scope.
- The clay widgets still rely on direct GDI pen/brush creation through the
  primitive path; broader UI migration should add caching before expanding to
  more repeated controls.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.12.b uses `WORLD_SIM_VERSION "0.2.12.b"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not change the binary
  save layout.
- Build/static validation and focused non-disruptive GUI validation were
  completed for the Phase 2A UI/resource scope, including app icon preservation
  after both Makefile and build.bat builds and collapsed-handle artifact checks.
- Strict AGENTS full game-flow regression was not completed for this
  presentation/resource checkpoint and should not be claimed for broad gameplay
  or performance acceptance.

## Ver0.2.12.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.12.a.
2. Added `src/ui/ui_clay_theme.c` and `src/ui/ui_clay_theme.h` for centralized
   Claymorphism theme tokens, colors, radius, spacing, shadow, highlight, and
   state text colors.
3. Added `src/ui/ui_clay_primitives.c` and
   `src/ui/ui_clay_primitives.h` for reusable clay surface drawing primitives.
4. Wired the first minimal clay proof into the side-panel shell and panel-tab
   backgrounds without changing panel content, hit testing, actions, or map
   body rendering.
5. Updated panel-tab text to use the clay theme text token instead of a local
   hard-coded color.
6. Synchronized `Makefile` and `build.bat` for the new UI clay source files.
7. Added the AGENTS rule for hwnd-scoped, non-activating GUI validation on
   another monitor, explicitly avoiding focus-stealing global input while the
   user may be using another foreground application.

Known follow-up:

- Phase 2 Claymorphism work can start from this foundation, but should stay
  scoped to top/bottom bars, buttons, tabs, and pause menu.
- Broader clay migration should add a widget or surface-cache layer before
  expanding repeated rounded shadows and stateful controls.
- Phase 6 large-map stutter and simulation-speed work remains outside this
  release scope.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.12.a uses `WORLD_SIM_VERSION "0.2.12.a"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not change the binary
  save layout.
- Build/static validation and focused non-disruptive GUI smoke validation were
  completed for the Phase 1 UI presentation scope. The strict AGENTS full
  game-flow regression was not completed and should not be claimed for broad
  gameplay acceptance.

## Ver0.2.12

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.12.
2. Added `src/core/city_display.c` and `src/core/city_display.h` as the
   shared display-coordinate boundary for normal city slots and port-city
   harbor markers.
3. Updated map rendering, label placement, selection highlighting, country
   focus, civ snapshot focus points, and map hit-testing so a port city uses
   its harbor marker as the visible/clickable/focused point instead of mixing
   city and harbor positions.
4. Drew port capitals with the same extra capital ring treatment used by
   ordinary capitals, while still rendering the harbor marker at the coast.
5. Updated the country overview metrics so the duplicate city count is replaced
   by port count; province count remains the total one-city-per-region count.
6. Reworked port policy so coastal candidate discovery is private/transient and
   `NaturalRegion.has_port_site` records the final actual port state only.
7. Reduced non-forced coastal port density while preserving the land-component
   island guarantee for at least one valid port.
8. Synchronized `Makefile` and `build.bat` source lists for the current module
   split.
9. Recorded the UI/UX Claymorphism presentation rules in `AGENTS.md`.
10. Cleaned module line counts back under the 500-line `.c` / `.h` limit
    without gameplay changes.

Known follow-up:

- Phase 6 large-map stutter and simulation-speed work remains outside this
  release scope.
- Full current-AGENTS GUI/game-flow strict regression remains required before
  treating this gameplay/rendering checkpoint as broadly accepted if it was not
  completed in the release task.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.12 uses `WORLD_SIM_VERSION "0.2.12"`.
- `MAP_SAVE_VERSION` remains 10 because this release does not change the binary
  save layout; it only changes settlement/port display and policy semantics
  within the existing saved region/city fields.
- Required release checks include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, file-size checks, root executable
  inventory, static keyword scans, focused GUI validation, and the current
  AGENTS strict regression for simulation/render/map-display changes.

## Ver0.2.11

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.11.
2. Rebalanced natural-region province sizing with explicit hard/soft size
   bands, target-fit merge scoring, and stricter huge-region splitting so
   province sizes can vary without extreme outliers.
3. Made natural regions the province ownership unit used by war cession
   accounting and transfer selection.
4. Added stable one-region-one-city settlement slots generated at worldgen,
   repaired after load/regeneration, and preserved across claim and war
   transfer.
5. Moved port-city assignment into a full region port-policy pass. Ports are a
   subtype of the one region city, not an extra settlement entity.
6. Applied coastal port weighting and forced at least one port city for island
   land components when a valid coastal candidate exists.
7. Updated settlement rendering so neutral generated city/port slots are
   visible on the Regions map layer but hidden on normal gameplay layers.
8. Exposed city overlay cache and neutral-settlement debug counters in the
   Debug / Performance panel.
9. Kept side-panel collapse, handle dirty-rect, legend, and UI-only repaint
   fixes in the release scope.
10. Updated AGENTS validation to require technology stage 5 and explicit
    deep-sea route hidden-before/revealed-after evidence for strict regression.

Known follow-up:

- Phase 6 large-map stutter and simulation-speed work remains outside this
  release scope.
- `docs/official` was not regenerated for this checkpoint; this release is
  recorded in the unofficial version log and side doc.

Validation notes:

- Ver0.2.11 uses `WORLD_SIM_VERSION "0.2.11"`.
- `MAP_SAVE_VERSION` is 10 because generated settlement/port-region state is
  repaired and normalized after loading.
- Required release checks include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, file-size checks, root executable
  inventory, static keyword scans, focused GUI validation, and the current
  AGENTS strict regression for simulation/render/map-display changes.

## Ver0.2.10.f

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.f.
2. Restored the strict Phase 6 and performance validation gate in `AGENTS.md`
   after the rejected Phase 6 experiments were rolled back to the
   Ver0.2.10.e baseline.
3. Reinstated the requirement that performance, stutter, scheduler, rendering,
   map-display, simulation-speed, or Phase 6 validation use a Large map, at
   least 26 placed civilizations, randomized physical map parameters,
   randomized advanced terrain preferences, more than 600 natural regions, and
   5x/max speed until at least five distinct civilizations reach technology
   stage 10.
4. Reinstated the requirement that final validation reports include final
   year/month, natural region count, confirmed civilization count, speed
   setting, and exact ids/names for five technology-stage-10 civilizations.
5. Reinstated the fullscreen-safety rule for executable validation: agents must
   not force `world_sim.exe` to the foreground when another fullscreen
   application is active, and must use another monitor or a non-disruptive
   background setup instead.
6. Reinstated the maximized-window validation rule so the Debug / Performance
   panel is fully visible or fully transcribed, rather than relying on cropped
   performance evidence.
7. Kept gameplay, RNG, balance, save format, diplomacy rules, war rules,
   plague rules, population math, maritime rules, route-potential rules, world
   generation semantics, rendering code, and simulation code unchanged.

Known follow-up:

- Phase 6 large-map stutter and simulation-speed work remains rejected and
  unsolved. Future work should restart from this Ver0.2.10.f baseline and use
  evidence-first diagnosis before making changes.
- `docs/official` was not regenerated for this instructions/version checkpoint
  release.

Validation notes:

- Ver0.2.10.f uses `WORLD_SIM_VERSION "0.2.10.f"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, and root executable checks are required before the release commit.
- Full GUI gameplay regression is not required for this release because it only
  changes repository instructions, version metadata, and release documentation;
  no gameplay, simulation, rendering, UI, world generation, diplomacy, war,
  plague, population, route, or map display code changed.

## Ver0.2.10.e

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.e.
2. Completed the Phase 5 RenderSnapshot cache-slimming pass by adding a
   simulation-side presentation cache module for snapshot-ready data.
3. Cached city region summaries and city population summaries outside the
   RenderSnapshot publish read-lock path.
4. Cached diplomacy relation, war state, war-front flags, and peace-pressure
   pairs outside the RenderSnapshot publish read-lock path.
5. Cached sea-lane snapshot rows and plague civ/city/lane presentation
   summaries outside the RenderSnapshot publish read-lock path.
6. Added a synchronous `render_snapshot_cache_update_all()` helper for
   simulation/write-side finalize paths that must publish immediately.
7. Primed the snapshot presentation cache before forced publishes after world
   generation finalize, load finalize, region regeneration, and manual
   simulation edits such as add/edit civilization, color changes, civil unrest,
   and vassal release.
8. Changed dead city cache entries inside `city_count` to become valid
   zero-summary entries for the current key so they no longer keep the city
   cache dirty forever.
9. Added Debug / Performance System rows for snapshot city cache, diplomacy
   cache, plague cache, and lane snapshot source state.
10. Kept gameplay, RNG, balance, save format, diplomacy rules, war rules,
    plague rules, population math, maritime rules, route-potential rules, and
    world generation semantics unchanged.

Known follow-up:

- Large-map stutter is improved around RenderSnapshot read-lock work, but it is
  not fully solved. Remaining targets include static layer rebuild chunking,
  sea-lane overlay layering, and further scheduler/write-lock slicing.
- The new synchronous cache-prime helper is intended only for forced publish
  finalize paths. Routine monthly cache refresh remains in the simulation
  scheduler path.
- `docs/official` was not regenerated for this render/cache checkpoint release.

Validation notes:

- Ver0.2.10.e uses `WORLD_SIM_VERSION "0.2.10.e"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, and executable GUI smoke/regression checks
  are required before the release commit.

## Ver0.2.10.d

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.d.
2. Split the side-panel view-model cache into tab/subview-aware cache kinds for
   collapsed, country list, country detail, population, plague, worldgen, debug
   map, and debug performance views.
3. Removed the global `render_snapshot_revision()` dependency from the default
   panel data key so unrelated snapshot publishes no longer invalidate every
   side-panel view.
4. Narrowed panel UI/data keys so unrelated map zoom, map legend, worldgen,
   debug, country, population, and plague state affect only the panels that
   display them.
5. Kept map zoom and map legend state in the World panel key because that panel
   displays those values.
6. Added a hover-only side-panel repaint path that does not force full panel
   cache invalidation.
7. Preserved full panel invalidation for real state changes such as clicks,
   tab changes, scroll changes, language changes, and selected-civ changes.
8. Added Debug / Performance System rows for panel cache key type, invalidation
   kind, full/hover invalidation counts, and throttle counts.
9. Fixed the Country Overview cache key so Recent Events updates when
   `events_revision` changes, without adding event invalidation to unrelated
   country detail tabs.
10. Kept gameplay, world generation, expansion, diplomacy, war, plague,
    population math, ports, maritime rules, sea-lane generation, route
    potential, save format, and balance unchanged.

Known follow-up:

- Large-map stutter is improved for side-panel hover and unrelated map
  interaction, but it is not fully solved. The next target is snapshot publish
  slimming so RenderSnapshot copying holds the read lock for less time.
- Hover-only repaint still draws the live side panel once for hover feedback.
  If hover remains expensive, future work can split tooltip/highlight overlays
  from the panel body.
- `docs/official` was not regenerated for this render/cache checkpoint release.

Validation notes:

- Ver0.2.10.d uses `WORLD_SIM_VERSION "0.2.10.d"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, and executable smoke launch are required
  before the release commit.

## Ver0.2.10.c

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.c.
2. Added a dedicated map label cache module that separates map-space label
   source/candidate data from screen-space placement.
3. Reduced `src/render/map_labels.c` to a small draw-entry wrapper while moving
   label cache implementation into `src/render/map_label_cache.c`.
4. Changed the label source key so it excludes camera, viewport, raw pan,
   mouse, and frame state.
5. Kept the label source key tied to label dirty revisions, map size,
   `city_visual_revision`, `regions_revision`, world generation state, and UI
   language.
6. Kept placement cache invalidation tied to placement concerns such as
   viewport/layout buckets, display mode, semantic zoom LOD, tile size, and
   selection.
7. Skipped full label placement/collision work during `map_interaction_preview`
   and allowed exact placement to rebuild after interaction settles.
8. Added label debug rows for source rebuilds, placement rebuilds, preview
   skips, drawn labels, and source/placement reasons.
9. Added `src/render/map_label_cache.c` to the canonical Makefile build.
10. Kept gameplay, world generation, expansion, diplomacy, war, plague,
    population math, ports, maritime rules, sea-lane generation, route potential,
    save format, and balance unchanged.

Known follow-up:

- Large-map stutter is improved for pan/zoom label work, but it is not fully
  solved. The next target is side-panel cache key splitting so map zoom, hover,
  unrelated snapshot publishes, and debug refreshes do not invalidate the whole
  right-side panel.
- `src/render/map_label_cache.c` is exactly 500 lines. Future label work should
  split it before adding logic.
- `docs/official` was not regenerated for this render/cache checkpoint release.

Validation notes:

- Ver0.2.10.c uses `WORLD_SIM_VERSION "0.2.10.c"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, and executable smoke launch are required
  before the release commit.

## Ver0.2.10.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.b.
2. Changed route overlay cache content keys so pan/zoom camera and layout state
   do not continuously invalidate route overlay content during interaction.
3. Changed city overlay cache content keys to use a city visual revision instead
   of population-driven city data revisions or raw tile revisions.
4. Added transformed preview reuse for route and city overlay bitmaps while
   `map_interaction_preview` is active.
5. Skipped full label candidate/layout work during interaction preview and
   forced an exact label rebuild after preview settles.
6. Split RenderSnapshot city revisions into data and visual revisions so
   population data can refresh without forcing city/capital/port icon redraws.
7. Narrowed `population_sync_all()` so it marks city visuals dirty only when a
   city crosses a visual population class threshold.
8. Added debug visibility for city data/visual/population revisions and overlay
   exact versus preview reuse counts.
9. Renamed the disorder pressure label from `Wartime pressure` to
   `War fatigue`.
10. Kept gameplay, world generation, expansion, diplomacy, war, plague,
    population math, ports, maritime rules, sea-lane generation, route potential,
    save format, and balance unchanged.

Known follow-up:

- Large-map stutter is improved during direct pan/zoom interaction, but it is
  not fully solved. The next target is a map-space label candidate cache with
  separate screen placement. Later work should split panel cache keys, slim
  snapshot lock-held time, rebuild static layers in stricter budgeted stripes,
  layer sea-lane overlays, and further reduce scheduler write-lock step size.
- `docs/official` was not regenerated for this render/cache checkpoint release.

Validation notes:

- Ver0.2.10.b uses `WORLD_SIM_VERSION "0.2.10.b"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, and executable smoke launch are required
  before the release commit.

## Ver0.2.10.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.a.
2. Replaced the packaged Windows app icon with the new parchment-map icon.
3. Regenerated `assets/app_icon.png` as a transparent 512x512 source image.
4. Regenerated `assets/app_icon.ico` as a multi-resolution Windows icon.
5. Kept the existing resource script, Makefile resource build, and window icon
   loading path unchanged.
6. Kept the release scoped to executable icon packaging and version metadata.

Known follow-up:

- Large-map stutter remains present. This checkpoint does not change gameplay,
  rendering behavior, simulation scheduling, snapshot copying, cache
  invalidation, or save format.
- `docs/official` was not regenerated for this icon-only checkpoint release.

Validation notes:

- Ver0.2.10.a uses `WORLD_SIM_VERSION "0.2.10.a"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, associated-icon extraction, and executable
  smoke launch are required before the release commit.

## Ver0.2.10

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.10.
2. Added a Windows app icon and resource script to the canonical executable
   build.
3. Changed diplomacy map animations to consume only events present in the
   currently rendered `RenderSnapshot`.
4. Added snapshot-consistent civ focus endpoints for diplomacy animation lines.
5. Validated diplomacy contact and war-front animation eligibility from
   snapshot relation/front state instead of newer live state.
6. Delayed new diplomacy map animations until snapshot-backed static map layers
   have presented the same snapshot.
7. Changed static map cache keys to use snapshot revision fields for
   snapshot-drawn pixels.
8. Guarded render dirty clearing so stale snapshot pixels cannot acknowledge
   newer live visual dirty revisions.
9. Added Debug / Performance System rows for diplomacy animation source,
   delayed state, consumed event totals, and snapshot/static/live map revisions.
10. Kept the release scoped to executable packaging, presentation ordering,
    render/cache semantics, and debug visibility.

Known follow-up:

- Large-map stutter remains present. Ver0.2.10 gates the diplomacy animation
  presentation race, but profiling still shows late-game pressure from plague
  animation, static-dirty invalidation, maritime drawing, label work, and
  simulation backlog.
- `docs/official` was not regenerated for this checkpoint release.

Validation notes:

- Ver0.2.10 uses `WORLD_SIM_VERSION "0.2.10"`.
- `MAP_SAVE_VERSION` remains 9 because this release does not change the save
  format.
- `make world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, root executable checks, and bounded GUI large-map validation are
  required before the release commit.

## Ver0.2.9.c

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.9.c.
2. Added a centralized plague performance switch module with default ON state
   for both the plague system and plague map visuals.
3. Added Debug / Performance System controls for `Plague System` and
   `Plague Map Visuals`.
4. Kept normal gameplay unchanged by default; plague simulation and visuals are
   disabled only when the debug switches are explicitly turned off.
5. When plague map visuals are OFF, skipped plague fog, pulse, and infected-lane
   map visuals and suppressed plague-animation map invalidation.
6. When the plague system is OFF, skipped random outbreaks, monthly plague
   update work, migration and war exposure, plague disorder contribution, and
   plague population death effects.
7. Added debug rows for plague system/visual switch state, skipped simulation,
   skipped visuals, suppressed invalidation, and last plague visual reason.
8. Kept render and simulation integration narrow, without changing world
   generation, expansion, diplomacy, war rules, maritime rules, sea-lane
   generation, route-potential algorithms, or default plague balance.

Known follow-up:

- Large-map stutter is still present. Turning plague visuals/system off helps
  isolate one source, but profiling still shows non-plague stalls in full
  viewport overlays, labels, sea lanes, calendar/scheduler work, diplomacy, war,
  and late-game map churn.
- With the plague system OFF, existing plague snapshot data is preserved rather
  than cleared. The switch disables ongoing plague simulation/effects and map
  visuals; it does not currently rewrite historical plague state in panels.

Validation notes:

- Ver0.2.9.c uses `WORLD_SIM_VERSION "0.2.9.c"`.
- `docs/official` was not regenerated for this checkpoint release.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size
  checks, and bounded GUI large-map validation are required before the release
  commit.

## Ver0.2.9.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.9.b.
2. Added a simulation-side DecisionSnapshot cache so RenderSnapshot civ copying no longer calls expensive decision diagnostics under the state read lock.
3. Added cached-only country and population summary readers for snapshot copying.
4. Split civ snapshot copying into `render_snapshot_civs.c` and added per-phase civ copy profiling.
5. Added budgeted monthly DecisionSnapshot cache refresh and eager refresh after world generation or map load.
6. Kept same-identity stale decision data when a fresh cache entry is not ready, with a safe Waiting fallback instead of zeroed contradictory data.
7. Fixed country decision labels so Waiting and Unknown do not render as Expansion by default.
8. Added debug rows for decision cache validity, dirty count, update time, and snapshot cached/stale/fallback counts.
9. Added viewport static, route overlay, and city overlay presentation caches as the second render stutter cleanup pass.

Known follow-up:

- Large-map stutter is still present. DecisionSnapshot no longer dominates RenderSnapshot publishing, but profiling still shows render-side spikes in full viewport overlays, labels, sea-lane drawing, and plague animation.

Validation notes:

- Ver0.2.9.b uses `WORLD_SIM_VERSION "0.2.9.b"`.
- `docs/official` was not regenerated for this checkpoint release.
- `make -B world_sim.exe`, `make check-text`, `git diff --check`, file-size checks, and bounded GUI large-map validation are required before the release commit.

## Ver0.2.9.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.9.a.
2. Retired the old screen-space map scene cache that mixed static geography with dynamic cities, ports, and maritime routes.
3. Kept maritime routes, plague visuals, city/capital/port icons, labels, highlights, and selected markers as live overlays over the static map cache.
4. Removed the default completed-month dynamic map redraw request so month advancement only invalidates the map when a visible map dirty flag is present.
5. Split label invalidation into narrower country and city/port label revision paths.
6. Changed population-only and civ-stat-only dirty paths so they no longer force label or city-icon layout rebuilds by default.
7. Added debug rows for completed-month map redraw, map invalidation reason, retired scene cache status, label revision components, icon counts, and route-geometry reuse.

Known follow-up:

- Large-map stutter is still present. Profiling shows remaining hot spots in RenderSnapshot civ-section publishing under the state read lock and in full viewport dynamic overlay rendering. This release records the cache-invalidation checkpoint before the next performance pass.

Validation notes:

- Ver0.2.9.a uses `WORLD_SIM_VERSION "0.2.9.a"`.
- `docs/official` was not regenerated for this checkpoint release.
- `make check-text`, `git diff --check`, `make all`, and bounded GUI large-map validation are required before the release commit.

## Ver0.2.9

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.9.
2. Made the global structured Event Log the canonical event source and changed country recent events to resolve through global event ids.
3. Added compact country and independent-country count cards to the country panel header area.
4. Added East/West civilization heritage data, bilingual name pools, and heritage-aware generated country/province naming.
5. Improved manual civilization randomization and custom color application.
6. Replaced UI icon assets/mapping with the new transparent strategy icon pack.
7. Added stability-decision gate display and reorganized the Decision tab into overview, expansion, war, and stability views.
8. Tightened UI presentation for disorder, decision countdowns, recent events, vassal cards, and single-province collapse outcomes.
9. Continued render/cache diagnostics and route/label/plague/side-panel cache-boundary work to reduce avoidable UI stalls.

Validation notes:

- Ver0.2.9 uses `WORLD_SIM_VERSION "0.2.9"`.
- `docs/official` was not regenerated for this release.
- `make check-text`, `git diff --check`, and `make` are required before the release commit.
- Probe and manual validation results are recorded in the release response.

## Ver0.2.8

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.8.
2. Added dynamic save/load state blocks for diplomacy, war, plague, and event history while preserving compatibility with older saves.
3. Added map-load progress state and the central load progress overlay.
4. Preserved post-load diplomacy, war, vassal, truce, plague, and event state instead of resetting those systems after a new save loads.
5. Added right-panel view-model caching and atomic panel redraw paths to reduce half-painted toolbar states.
6. Split static map cache layers and added a non-plague scene cache so plague-only animation does not redraw routes, cities, and labels every frame.
7. Lowered plague fog rebuild frequency, used an adaptive lower-resolution fog cache, and exposed real plague visual metrics.
8. Added smoother sea-lane/plague visual diagnostics and scene-cache debug metrics.
9. Softened political map colors with render-time saturation limiting and lower political fill alpha.
10. Rebuilt shallow/deep water depth from a smooth coast-distance and shelf-width field instead of per-tile noisy shelf spikes.
11. Added water-depth debug rows for shallow/deep counts, cache rebuild time, and shelf-width range.
12. Kept water logic as shallow sea, deep sea, and land-only; no lake/bay/ocean gameplay categories were added.

## Ver0.2.7

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.7.
2. Added Ver0.2.7 official documentation and change-summary documents under `docs/official`.
3. Added a Ver0.2.7 side doc under `docs/unofficial`.
4. Added per-civilization gameplay event history keyed by stable civ identity.
5. Moved country recent events to snapshot-backed per-civ event history.
6. Made right-side panel partial repainting atomic through an offscreen buffer.
7. Reduced simulation-driven side-panel invalidation and hover redraw churn.
8. Aligned world-generation progress stages with the actual generation flow.
9. Adjusted progress weights so deep-route completion leaves only finalize work.
10. Updated map legend rules for political, geography, climate, region, and route-potential layers.
11. Kept map water legend terminology limited to shallow sea and deep sea.
12. Changed normal war outcome truces to 55 years.
13. Changed severed-front and fallback war truces to 25 years.
14. Preserved collapse split truces at 45 years.
15. Added initial-truce duration data for correct diplomacy-card progress bars.
16. Cleaned remaining diplomacy-card mojibake strings touched by this release.

## Ver0.2.6.b

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.6.b.
2. Added Ver0.2.6.b official documentation and change-summary documents under `docs/official`.
3. Added a Ver0.2.6.b side doc under `docs/unofficial`.
4. Added the custom HSV color picker flow with exact manual color apply, cancel safety, auto-avoid preview, and modal pause behavior.
5. Unified player-facing water rendering around shallow sea and deep sea snapshot data.
6. Polished world-generation progress display so the central overlay owns the full progress bars and the side panel only shows concise status.
7. Reduced river visual weight and added river geometry/cache LOD diagnostics.
8. Improved natural-region validation and shape repair diagnostics for long strips and slivers.
9. Added priority-based map label collision handling so country, capital, city, and province labels resolve by importance.
10. Kept render/UI changes snapshot-oriented and avoided gameplay rule changes.

## Ver0.2.6.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.6.a
2. Added Ver0.2.6.a official documentation and change-summary documents under `docs/official`
3. Added a Ver0.2.6.a side doc under `docs/unofficial`
4. Strengthened AGENTS release/verification rules to reduce missed version, docs, tag, and push steps
5. Added snapshot-backed country/UI read helpers and country card support already present in this patch workspace
6. Added natural-region validation/config helpers for size distribution and shape diagnostics
7. Added a dedicated hydrology dirty flag and render revision
8. Moved river rendering out of the border cache into a dedicated map-space hydrology cache
9. Added render-side river geometry smoothing and diagnostics without changing river gameplay data
10. Added hierarchy-aware, terrain-aware, multi-pass river rendering with subtle mouth caps
11. Added Debug hydrology rows for river counts, lengths, confluences, invalid uphill segments, dead ends, short rivers, and cache rebuild timing
12. Verified `git diff --check` and `make` for Ver0.2.6.a

## Ver0.2.6

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.6
2. Added Ver0.2.6 official documentation and change-summary documents under `docs/official`
3. Added stable bilingual province-name data and region/province name ID handling
4. Improved province label readability and language switching
5. Changed the default player-facing map layer to Political and removed the ordinary All-layer option
6. Split world-generation progress into overall and current-stage progress bars
7. Kept the right sidebar visible during world generation while masking only the map area
8. Tuned route-potential shallow and deep sea-lane rendering colors, widths, dash rhythm, and draw order
9. Tightened deep sea route potential into a sparse backbone over shallow networks
10. Added unified diplomacy contact checks for land, active shallow sea networks, active deep sea networks, and vassal proxy contact
11. Prevented peace, tension, war starts, and diplomacy map animations from appearing without current contact
12. Made disconnected known peace/tension relationships cool down and eventually return to no active relationship
13. Updated Makefile/build source lists for the new progress, province-name, country-event, debug-worldgen, and diplomacy-contact modules
14. Verified `git diff --check` and a full temporary-target build for Ver0.2.6

## Ver0.2.5

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.5
2. Moved historical side docs and design references under `docs/unofficial`
3. Added official Ver0.2.5 universal documentation and change-summary documents under `docs/official`
4. Unified player-facing water categories around shallow sea and deep sea while preserving hard gameplay thresholds
5. Added visual shallow-to-deep water blending without changing route, expansion, or technology logic
6. Added a route-potential graph that precomputes potential port nodes and shallow/deep route edges after world generation
7. Made occupied port-site regions deterministically activate their route-potential port node
8. Rebuilt ordinary visible sea lanes from occupied route-potential nodes instead of random city-port discovery
9. Added a route-potential map layer and sea-lane diagnostics for active ports, hidden endpoints, and lane-cap skips
10. Tightened World-tab random buttons, input repainting, map centering, sidebar collapse, and legend clipping
11. Updated README, AGENTS documentation paths, ESC version log text, and the tracked version marker
12. Verified `make` and the 500-line `.c/.h` rule

## Ver0.2.4.a

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.4.a
2. Preserved the Ver0.2.4 diplomacy, event log, vassal display, and validation changes
3. Tightened repository agent rules for future work around planning, PATH setup, structured logs, snapshots, hot paths, localization, locked build outputs, and gameplay constants
4. Verified `make` and the 500-line `.c/.h` rule

## Ver0.2.4

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.4
2. Reworked diplomacy relation cards so peace/tension, war/truce, and tribute/vassal states use clearer visual templates
3. Displayed real vassal resource tribute from the six actually transferred resource fields at 40%
4. Displayed vassal total soldiers, callable 70%, home guard, current support use, and support casualties
5. Added a small diplomatic easing path and visible peace-return thresholds for tense relations
6. Moved event log storage/formatting into a structured bilingual event module
7. Fixed snapshot event copying so event text and clickable country payload come from the same event entry
8. Prevented historical event chips from highlighting a newly reused country slot by checking civilization uid
9. Added `world_sim.exe --probe-tech10` for deterministic technology stage 10 validation
10. Verified `make`, no `.c/.h` file over 500 lines, expansion probe, tech10 probe, and executable smoke launch

## Ver0.2.3

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.3
2. Added a first-position ESC menu Resume Game button so players can close the menu without pressing ESC again
3. Changed Save Map to open a Save As dialog, allowing the player to pick the destination folder and rename the `.wsgmap` file
4. Updated the ESC Version Log and README for the new pause/save behavior
5. Added the Ver0.2.3 side document
6. Verified the project builds with `build.bat`

## Ver0.2.2

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.2
2. Centralized vassal relationship handling in a dedicated simulation helper module
3. Prevented vassals from acting as independent diplomatic war actors and routed attacks on vassals to their overlord
4. Transferred nested vassals to the new overlord when a country becomes a vassal
5. Released all direct vassals when an overlord collapses and made vassal collapse successors independent
6. Added direct-vassal governance burden to disorder using `min(100, 8n + 3n*n)`
7. Applied 40% non-money resource tribute by deducting it from vassal resource output and adding it to the overlord
8. Added vassal support troops to wars with unified casualty tracking and per-front display of support losses
9. Included direct vassal provinces in war cession pools while preserving at least one province for each vassal
10. Updated Country and Diplomacy UI labels for vassal, tribute, no-autonomy, callable army, tribute, and governance burden states
11. Verified the project builds with `build.bat`
12. Verified no `.c` or `.h` file exceeds the 500-line rule
13. Verified no `.c` file includes another `.c` file

## Ver0.2.1

Implemented fixes:

1. Bumped the active prototype version to Ver0.2.1
2. Smoothed maritime route rendering with render-side curve subdivision while keeping simulation route paths unchanged
3. Added an ownership-revision safety check for political and border layer caches so claimed regions recolor immediately
4. Marked territory and label layers explicitly after natural-region claims
5. Clarified Country detail modifier rows so resource and technology percentages show the combined technology/disorder result and component values
6. Updated the ESC Version Log and README for Ver0.2.1
7. Verified the project builds with `build.bat`

## Ver0.2.0

Implemented features and fixes:

1. Bumped the active prototype version to Ver0.2.0
2. Added continuous mountain-chain generation with main ridges, branches, foothills, and debug metrics
3. Strengthened natural-region boundary costs so mountains, canyons, rivers, coasts, and terrain transitions shape regions more strongly
4. Added civilization technology stages 1-10 with innovation/resource/pressure-driven progress and localized stage names/effects
5. Connected technology effects to expansion speed, resource output, deep-sea route stability, defense, battle odds, and long-held vassal annexation
6. Rebalanced expansion so adjacent unowned natural regions remain the first priority before overseas expansion is considered
7. Converted disorder to a 0-100 scale with monthly pressure/recovery, war-death impact, plague-death impact, and decade collapse checks
8. Added collapse behavior that preserves the old capital remnant while splitting non-core natural regions into successor states
9. Reworked active wars into 3-year battles based on current soldiers, technology modifiers, population casualties, peace pressure, and 20% bordering-province cession
10. Added a Country Dashboard back-to-list control so selected-country detail and all-country list modes are clearly separated
11. Updated README and version metadata for Ver0.2.0
12. Verified the project builds with `build.bat`

## Ver0.1.8

Implemented fixes:

1. Bumped the active prototype version to Ver0.1.8
2. Split the monthly simulation scheduler further so expansion runs by civilization instead of as one whole-map phase
3. Split plague, random event, territory, diplomacy, and calendar work into separate monthly scheduler phases
4. Added speed-aware scheduler budgets so normal and fast speeds can process more bounded work per frame
5. Skipped monthly territory recalculation when expansion did not change city count or territory revision
6. Reworked the render cache into separate terrain, political, coast, and border/static-map layers
7. Moved maritime routes and plague overlays out of the expensive static map cache
8. Added render-only plague visual interpolation so outbreaks pulse, fade, and breathe between monthly simulation ticks
9. Changed plague map visualization from flat tile/province tint to soft dark-green infection clouds around infected cities
10. Made infected maritime routes use smoothly fading plague visual intensity instead of jumping directly with monthly exposure values
11. Kept plague visual code read-only from simulation state
12. Fixed Chinese population-pyramid labels for age structure, male, female, children, working age, elderly, fertile, recruitable, and pressure
13. Updated `Makefile` and `build.bat` for the new render and scheduler modules
14. Verified the project builds with `build.bat`
15. Verified no `.c` or `.h` file exceeds the 500-line rule
16. Verified no `.c` file includes another `.c` file

## Ver0.1.7

Implemented fixes:

1. Bumped the active prototype notes to Ver0.1.7
2. Added a fast cached-map preview path for mouse-wheel zoom and right-button map dragging
3. Deferred expensive high-quality map layer rebuilds until zoom or drag input settles
4. Cached the 800x600 base terrain bitmap by display mode and visual revision, so pan and zoom no longer regenerate base tile pixels
5. Moved map labels out of the expensive cached map layer, preventing stale country labels after civilization edits
6. Incremented the visual revision when civilization names, symbols, or visible traits are edited
7. Reused a full-window paint backbuffer instead of creating and deleting a compatible bitmap for every paint
8. Added event-style maritime route dirty marking and route ensuring
9. Stopped route rebuilds from calling `maritime_reset()`, avoiding duplicate route clearing and duplicate visual revision bumps
10. Marked maritime routes dirty when a city becomes a port and rebuilt them only when needed
11. Added diplomacy contact dirty marking so border/contact cache scans are skipped when territory contacts have not changed
12. Added a territory hash in the monthly recalculation path so diplomacy contact scans are marked dirty only when owner/province data changes
13. Kept population, plague, maritime, and overseas expansion systems in place because they were explicitly requested gameplay features before this performance pass
14. Skipped full-map plague overlay allocation and tile scanning when no city currently has active plague
15. Verified the project builds with `build.bat`
16. Verified no `.c` or `.h` file exceeds the 500-line rule
17. Verified no `.c` file includes another `.c` file
18. Added a fixed-frame runtime tick so rendering stays decoupled from month-speed intervals
19. Added a simulation scheduler wrapper that caps auto-run backlog to one pending month per frame
20. Added central dirty-flag APIs for world, territory, province, population, plague, maritime, labels, and render layers
21. Marked dirty state at world reset, territory changes, civilization edits, population changes, plague changes, and maritime route changes
22. Split population-only cache invalidation from region/territory invalidation so births, deaths, casualties, and migration update summaries without rebuilding province ownership caches
23. Moved city markers and plague city cores out of the expensive cached map layer so dynamic population visuals redraw cheaply
24. Kept the old `simulate_one_month()` entry point available while routing auto-run through the scheduler foundation
25. Added selectable active map sizes: Small 640x360, Medium 800x450, and Large 960x540
26. Changed startup to a blank ungenerated map state so the player explicitly generates from the Map tab or with F5
27. Changed generation defaults to centered sliders at 50 and initial civilizations at 0
28. Added active map dimension globals over max-size 960x540 static storage so the first variable-size pass avoids a risky full dynamic allocation rewrite
29. Updated world generation, rendering layout, tile hit tests, rivers, ports, and simulation scans to use the active map dimensions

## Ver0.1.6

Implemented features and fixes:

1. Bumped the active prototype version to Ver0.1.6
2. Added smoother cartographic map rendering layers for political fills, coast halos, country borders, province borders, coastlines, map labels, and subtle grid overlays
3. Added a render-layer cache so ordinary repaint events do not rebuild the full map surface every time
4. Added explicit river path objects for continuous visual river rendering while keeping the tile `river` flag for resource logic
5. Restyled city, capital, harbor, hill, and mountain map markers toward an old political-map look while reusing existing icon assets
6. Added map labels for countries and cities with simple overlap avoidance
7. Rebalanced diplomacy formulas so prosperous, resource-rich neighbors can still gain peaceful trade stability
8. Added explicit maritime route paths between ports with dashed sea-lane rendering
9. Added route-based port migration, maritime diplomacy contact, maritime trade contribution, and scoped overseas expansion from ports
10. Added city-level age and sex population cohorts with derived country population totals
11. Added population pressure, carrying-capacity pressure, cohort migration, soldier casualty population loss, and an Info-tab population pyramid
12. Replaced flat plague population loss with persistent city outbreaks, percentage deaths, spread pressure, disorder impact, immunity, and dark green map visualization
13. Added infected maritime route tint hooks for plague exposure along sea lanes
14. Reduced duplicate monthly maritime route rebuilds by coordinating route refresh from the simulation tick
15. Cached diplomacy border-contact statistics so monthly diplomacy no longer scans the full map once per civilization pair
16. Cached population country summaries so monthly systems and panels do less repeated aggregation
17. Split shared core declarations into `constants.h`, `world_types.h`, and `sim_types.h`, keeping `game_types.h` as the compatibility entry point
18. Removed unnecessary render-to-world-generation and world-port-to-game-state header coupling
19. Added and implemented the targeted diagnostic for map border, label, performance, and river-polish work
20. Reduced wall-like country/coast/province outlines with lower alpha, thinner screen-space strokes, and stricter province-border zoom gates
21. Reduced low-zoom label clutter and made country/city labels clearer with lighter outlines and stricter label priority
22. Made visual river strokes thinner with less jitter
23. Tightened river validity so main rivers directly reach ocean, bay, or lake, while tributaries must join an accepted river or directly reach water
24. Verified the source tree keeps every `.c` and `.h` file under 500 lines

## Ver0.1.5

Implemented features:

1. Bumped the active prototype version to Ver0.1.5
2. Added a dedicated right-side Diplomacy tab separate from the civilization editing tab
3. Diplomacy now lists only civilizations that have been contacted by the selected country
4. Each contacted relationship shows relation score, status, border tension, trade fit, resource conflict, border length, natural barrier, years known, and truce years
5. War relationships display a two-color progress bar using the selected country on the left and the opponent on the right
6. The war progress bar includes current soldiers, losses, and battle wins for both sides
7. Added read-only war soldier accessors so rendering can show military state without mutating simulation data
8. The Diplomacy tab shows available soldiers, mobilization base, capital garrison estimate, and province-level garrison estimates
9. Province military values are displayed as estimated garrisons because the current simulation does not yet store persistent per-province armies
10. Updated the right-side tab layout so Info, Civ, Diplomacy, and Map are all reachable
11. Updated README current-version notes for the Ver0.1.5 diplomacy UI
12. Kept the year/month top bar above the map draw pass so it is not covered by the map surface
13. Suppressed background erasing to reduce white flashes while switching tabs or resizing the side panel
14. Replaced city markers with the matching-style outpost, village, town, city, capital, and harbor icons
15. Replaced diplomacy factor text rows with icon metric blocks and hover labels
16. Localized compact metric labels to Chinese two-character labels in Chinese mode
17. Moved source files into responsibility folders: `core`, `game`, `world`, `sim`, `render`, and `ui`
18. Split `render.c` into map, panel, diplomacy, shared render helper, and icon modules
19. Split river generation into `src/world/rivers.c` and `src/world/rivers.h`
20. Added the 500-line `.c` and `.h` rule to `AGENTS.md` and verified the source tree follows it
21. Renamed design PDFs to versioned filenames: `ver0.1.5_province_expansion_logic.pdf`, `ver0.1.5_diplomacy_war_framework.pdf`, and `ver0.1.4a_code_review_notes_for_codex.pdf`
22. Updated `Makefile`, `build.bat`, README build commands, and source layout docs for the categorized tree
23. Reduced short scattered river fragments by moving river carving into a dedicated river-generation pass with minimum length and spacing checks

## Ver0.1.4.a

Freeze notes:

1. Replaced covered UI icons with the matching-style icon package while keeping existing resource icons for resource types not included in the new package
2. Added root `Makefile` using the current module list
3. Added the province expansion and diplomacy/war framework PDFs to `docs`
4. Tightened module includes so world generation no longer depends on the port or simulation boundary
5. Confirmed the first-stage refactor structure and kept `AGENTS.md` in place for future agent rules

## Ver0.1.4

Implemented features:

1. Bumped the active prototype version to Ver0.1.4
2. Fixed default civilization seeding beyond eight civilizations by expanding names, symbols, and trait presets to the full civilization limit
3. Restored province borders with a separate alpha border layer for smoother internal province and country outlines
4. Rebuilt the selected information panel around country and province sections instead of bold generic labels
5. Restored country population, land, city count, disorder, and province resource metrics
6. Added visible factor hints and hover labels for aggregate values such as population, disorder, and habitability
7. Added a bottom-right map legend for geography and climate colors
8. Strengthened the desert slider so dry climate generation changes more clearly
9. Changed world generation to reusable fractal value-noise fields for larger, more coherent land, moisture, and temperature regions
10. Made Ocean 0 produce land-first maps instead of shallow-water noise
11. Added `src/world/noise.c` and `src/world/noise.h` for map noise generation
12. Added `src/sim/diplomacy.c`, `src/sim/diplomacy.h`, `src/sim/war.c`, and `src/sim/war.h` as the first diplomacy and war structure
13. Added first-pass diplomacy contact tracking when living civilizations touch borders
14. Updated build documentation for the new module layout
15. Widened the default and minimum side panel so four metric blocks fit without squeezing
16. Switched icon rendering to draw PNGs through GDI+ directly so transparent icons keep their alpha channel
17. Expanded climate categories to tropical rainforest, monsoon, savanna, desert, semi-arid, mediterranean, oceanic, temperate monsoon, continental, subarctic, tundra, ice cap, alpine, and highland plateau
18. Expanded geography categories to the current design set: ocean, coast, plain, hill, mountain, plateau, basin, canyon, volcano, lake, bay, delta, wetland, oasis, and island
19. Added separate ecology and resource layers so a tile can have geography, climate, ecology, and resource features at the same time
20. Made province shapes grow with geography-aware frontier costs instead of simple circular city radii
21. Added ecology and resource names to selected province inspection
22. Smoothed province and country outlines through a higher-resolution border alpha surface
23. Added `src/data/game_tables.h` and `src/data/game_tables.c` for editable geography, climate, ecology, resource, and civilization metric tables
24. Pruned unsupported geography entries from generation tables so the code follows the current design charts more strictly
25. Added first-pass language switching for the main UI text path and selected-info panel labels
26. Started replacing old A/E/D/C traits with governance, cohesion, production, military, commerce, logistics, and innovation
27. Replaced the old four terrain sliders with world controls for ocean, continent fragmentation, relief, moisture, drought, vegetation, and advanced terrain bias
28. Connected every world-generation slider to the fractal map generator so rebuilding the world reflects the selected values
29. Added dynamic adaptation as a changing civilization state derived from environment, resources, culture, and disorder instead of a fixed core metric
30. Tightened the geography list to the approved table and kept Island as the only island category
31. Added easy-edit text reference tables to `src/data/game_tables.h` for the map pipeline, geography, climate, resources, and civilization metrics
32. Updated expansion so nearby claims attach to an existing city province while distant high-value targets can create frontier cities and provinces
33. Added first-pass alliance and vassal diplomacy states, with vassal outcomes available after severe war defeats

## Ver0.1.3

Implemented features:

1. Bumped the active prototype version to Ver0.1.3
2. Made right-panel stat labels true hover tooltips instead of persistent bottom overlays
3. Kept fixed province shapes after a city establishes its administrative region
4. Stopped city population growth from reshaping existing province borders
5. Added fixed province ids to map tiles so province ownership and province shape are separate concepts
6. Disabled active conquest during expansion for this prototype pass
7. Changed civilization expansion so contacted civilizations form borders and expand toward unowned land instead
8. Made newly founded cities immediately lock in their province region
9. Prevented new cities and starting civilizations from being placed inside an existing province shape
10. Reorganized source files into `src/core`, `src/world`, and `src/gui`
11. Added a shared `version.h` marker for Ver0.1.3

## Ver0.1.2

Implemented features:

1. Larger 1920x1080 graphical window
2. Higher-detail map grid
3. More random world seeding between launches
4. Month-based simulation
5. Space toggles pause and run
6. Three speeds
7. Right-side form for adding civilizations during simulation
8. Right-side form for editing selected civilization attributes
9. City system with capital cities and resource-based control radius
10. City capture transfers the controlled region
11. New terrain types: icefield and wetland
12. Terrain resource values and local attack/defense modifiers
13. Keyboard shortcuts for form actions: F1 add civilization, F2 apply selected, F5 new world
14. Land/ocean ratio slider for new worlds
15. Initial civilization count option
16. Draggable side panel divider
17. More reliable continent generation when rebuilding the world
18. Bottom play/pause and speed buttons
19. Better global shortcut handling when form controls have focus
20. Speeds changed to 1, 0.25, and 0.05 seconds per month
21. Mouse wheel zoom centered on the cursor
22. Height, moisture, and temperature based map generation
23. More coherent coastlines and terrain regions
24. Hills and rivers
25. Tile-level resource variation shown as numbers
26. Administrative region summaries when clicking city-controlled land
27. Split the code into categorized modules: `game_state.c`, `world.c`, `render.c`, and `ui.c`
28. Removed `1`, `2`, and `3` as keyboard speed shortcuts so numeric input fields can accept numbers
29. Added separate geography and climate layers with mode buttons
30. Added all-map, climate, geography, and political display modes
31. Added thin province borders alongside thick country borders
32. Added first-pass capital fall handling with relocation or collapse based on stability
33. Replaced the unity wrapper with real `.c/.h` module pairs for shared state, world logic, rendering, and UI
34. Smoothed the rendered map surface with interpolated colors instead of enlarged tile rectangles
35. Reduced repetitive tile texture marks that made the map read like a grid
36. Changed province display to use nearest city ownership inside each country, avoiding circular city-radius borders
37. Added a minimum city distance so cities and provinces spread out more naturally
38. Added right-side tabs for selected info, civilization management, and map generation
39. Moved map view mode buttons into the Map tab
40. Added generation sliders for ocean, mountains, desert, forest, and wetland
41. Changed cold climate generation to use random cold centers instead of fixed north/south poles
42. Increased the default map zoom so the world reads larger on first open
43. Increased the map grid to 240x135 for a clearer world without blurred interpolation
44. Restored crisp tile rendering instead of smoothed color interpolation
45. Added right mouse drag panning for moving around the map
46. Fixed collapsed countries leaving zombie territory after their capital falls
47. Made capital/province capture trigger when any tile in the province is successfully conquered
48. Added compact image-icon stat blocks with hover labels for country, tile, combat, and province resources
49. Added generated city/province names
50. Added elevation-based color variation so same geography can still show local differences
51. Removed the old Overview map mode button and made All the first map view
52. Widened right-panel stat blocks so icons and values do not overlap
53. Added the provided PNG icon set as project assets for population, combat, expansion, defense, culture, geography, climate, habitability, and resources
54. Added livestock as a separate terrain and province resource
55. Reserved a wider minimum side panel width so stat blocks and controls do not squeeze together when resized
56. Cached province ownership and province summaries to avoid repeated city scans during rendering and tile inspection
57. Culled off-screen map rendering so panning and zooming only draw visible tiles, rivers, and borders
58. Combined monthly resource scoring into one map scan for all civilizations instead of one full scan per civilization
59. Increased the internal map grid to 800x600 landscape tiles
60. Replaced per-tile base rendering with a crisp bitmap surface for better performance on high-density maps
61. Updated build instructions to use `-O2` optimization
62. Added the new territory, disorder, migration, and economy PNG icons to the UI icon registry
63. Moved the high-density smoothing buffer out of the stack to avoid large-map stack pressure
64. Rebalanced the ocean slider so Ocean 0 creates mostly land instead of dense shallow-water noise
65. Enlarged high-density terrain features so generated maps read less speckled
66. Added editable geography, climate, ecology, resource, and civilization metric tables in `src/data`
67. Pruned unsupported geography entries so generation follows the current design table only
68. Added first-pass language switching for English and Chinese UI labels
69. Added the seven civilization metrics: governance, cohesion, production, military, commerce, logistics, and innovation
70. Replaced the old UUID icon set with the new crisp semantic icon package
71. Added climate generation inputs for randomized latitude, distance to sea, elevation cooling, and mountain rain shadow

## Ver0.1.1

Implemented features:

1. Windows graphical game window
2. Colored terrain rendering
3. Colored civilization territory overlay
4. Country border lines
5. Coast outline lines
6. Village icons
7. Year display
8. Side panel with civilization stats
9. Mouse tile inspection
10. Keyboard controls for simulation, auto-run, reset, and quit

## Ver0.1.0

Implemented features:

1. Procedural world generation
2. Terrain types: ocean, coast, grassland, forest, desert, mountain
3. Three default civilizations
4. Custom civilization creation
5. Editable civilization preferences
6. Population growth
7. Territory expansion
8. Border conflict and conquest
9. Random world events
10. Tile inspection and civilization status views

## Ver0.0.1

Goal:

Create the first text based civilization simulation prototype.

Planned features:

1. Create three civilizations
2. Simulate population growth
3. Simulate territory expansion
4. Simulate simple battles
5. Print daily results
