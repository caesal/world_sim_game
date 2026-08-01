# Ver0.3.6.c Side Doc

Ver0.3.6.c is the measured climate-envelope presentation checkpoint over
Ver0.3.6.b. It preserves the existing world-generation control UI and legacy
integer interface while replacing the first illustrative climate background
with the user-approved C2 continuous tendency field.

This release intentionally does not update `docs/official`; that directory is
the separately requested official documentation-freeze series. The release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Preserved Control Contract

- Physical Terrain, Climate & Vegetation, Hydrology & Regions, and Legacy
  Modules remain the four synchronized world-generation tabs.
- The climate quadrilateral retains four independently draggable,
  quadrant-constrained handles on the existing `-50..+50` plane.
- The handles continue to decompose into the same forest, desert, moisture,
  and drought integer fields used by Legacy Modules.
- The seven-axis world fingerprint, physical controls, hydrology controls,
  natural-region presets, Initial Civilizations synchronization, manual
  civilization workflow, dice, reset, and F5 generation path are unchanged.

## Calibration Design

- The automation enumerates 28,561 effective climate-envelope configurations.
- Every configuration is measured at Small, Medium, Large, and Extreme map
  sizes.
- Four calibration seeds build the frozen model; two independent holdout seeds
  test it without participating in fitting.
- Formal evidence contains 456,976 calibration worlds and 228,488 holdout
  worlds, for 685,464 generated worlds in total.
- A separate 114,244-world qualification run proved enumeration, interruption,
  resume, and deterministic-repeat behavior before the formal run.
- The weighted quadrilateral interpretation represents 9,375,000 conceptual
  raw cases without pretending that those weighted cases were separate world
  generations.

The frozen calibration-model SHA-256 is
`048A7C4198E88A18F596997B5E914CB02276D622989254A7AA08B0F2BCA01812`.
The formal raw-manifest SHA-256 is
`A5D9F42AE9ED5C94DA31B9C2DE259B84F0A8C46444876EF7B7BABAD6E11DA388`.

## Statistical Findings

- 79 of 81 measured coordinates are cross-stable across all six seeds and all
  four size-specific surfaces.
- The two mixed coordinates are `(-50, 0)` and `(50, 50)`; the C2 background
  keeps those transitions blended rather than drawing a hard categorical edge.
- Calibration-versus-holdout top-tendency agreement is 100%.
- Coordinate rank agreement is `0.993533`; global lift rank agreement is
  `0.997756`.
- The highest tendency does not change by map size at any measured coordinate.
- The measured adapter does not support presenting the horizontal axis as a
  robust direct temperature control or the vertical axis as a precise monotonic
  dry/wet forecast. Those labels remain broad control tendencies only.

## C2 Presentation

- Icefield, tundra, temperate grassland, desert, forest, monsoon, and tropical
  rainforest remain visible as broad tendency regions.
- Desert and temperate-grassland areas receive the visual prominence supported
  by the measured surfaces.
- Smaller icefield, tundra, forest, monsoon, and tropical-rainforest areas are
  retained in plausible locations instead of being deleted.
- Soft continuous blends communicate uncertainty and avoid claiming that one
  pixel predicts a generated biome.
- The image remains a presentation asset. It is never read by world generation
  and never changes simulation state.

## Automation And Evidence

- C worker modules own deterministic probing, metric collection, and bounded
  CSV output.
- Python tooling owns enumeration, resumable orchestration, integrity checks,
  evidence processing, holdout comparison, lineage, and summary generation.
- Raw generation evidence is kept outside the product source. The primary
  repository retains the raw-free processed evidence needed to audit the
  statistical conclusions.
- Focused C2 presentation evidence covers the final asset, panel layout,
  interaction geometry, cache behavior, and English/Chinese narrow and wide
  artifacts.

## Save And Compatibility

- `WORLD_SIM_VERSION` is `0.3.6.c`.
- `MAP_SAVE_VERSION` remains `20`.
- No save field, block, payload, migration, or rejection policy changes.
- World-generation algorithms, phase order, random streams, map semantics,
  simulation rules, and drawing order remain unchanged from Ver0.3.6.b.

## Release Validation Contract

- Final release acceptance requires incremental and focused probes, the
  task-specific matrices, both canonical build paths, static/text/source-list
  checks, performance/resource checks where applicable, non-activating
  generated-world GUI evidence, a final diff audit, and one fresh final-source
  Rule 39 run as the last gate.
- Historical calibration and C2 visual evidence remain useful supporting
  evidence, but only validation bound to the final release source can establish
  the release gate.
