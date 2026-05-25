# Ver0.2.10.b Side Doc

Ver0.2.10.b is a render-interaction and city-visual cache checkpoint over
Ver0.2.10.a. It keeps the Ver0.2.10.a icon packaging and gameplay behavior, but
narrows several presentation cache invalidation paths that were still tied to
camera movement or ordinary population data refreshes.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Route overlay content keys no longer use pan/zoom camera or screen layout
   state.
2. City overlay content keys now use `city_visual_revision` instead of the
   population-sensitive city data revision.
3. During `map_interaction_preview`, route and city overlay bitmaps are reused
   through transformed preview presentation instead of being rebuilt for every
   wheel or drag step.
4. During `map_interaction_preview`, labels skip full candidate/layout work and
   rebuild exactly after the interaction settles.
5. RenderSnapshot now carries separate city data and city visual revisions.
6. Population synchronization marks city visuals dirty only when city visual
   population class changes.
7. Debug output distinguishes city data, visual, and population revisions and
   route/city exact versus preview overlay reuse.
8. The disorder pressure label is now `War fatigue`.

## No Gameplay Changes

This release does not change map generation, province generation, expansion,
diplomacy, war, plague, population math, ports, maritime rules, sea-lane
generation, route-potential algorithms, save format, or balance constants.

## Known Follow-Up

Large-map stutter is still not fully solved. The next planned step is a
map-space label candidate cache with a separate screen placement pass so pan
does not rebuild label sources and wheel preview can continue reusing accepted
labels. Later candidates include splitting panel view-model cache keys, slimming
snapshot lock-held work, budgeted static layer stripes, sea-lane overlay
layering, and smaller scheduler write-lock steps.

## Validation

- `WORLD_SIM_VERSION` is `0.2.10.b`.
- `MAP_SAVE_VERSION` remains 9.
- Build and release checks should include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, `.c/.h` line-count checks, root
  executable inventory, and an executable smoke launch.
