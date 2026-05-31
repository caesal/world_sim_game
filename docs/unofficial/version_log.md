# Version Log

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
