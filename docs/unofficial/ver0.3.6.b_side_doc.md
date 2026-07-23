# Ver0.3.6.b Side Doc

Ver0.3.6.b is the world-generation control UI and repository-governance
checkpoint over Ver0.3.6.a. It deliberately preserves the existing generator
and exposes its existing integer fields through a more visual, synchronized
control surface.

This release intentionally does not update `docs/official`; that directory is
the separately requested official documentation-freeze series. The release
record lives in this side doc and `docs/unofficial/version_log.md`.

## World Setup Shell

- World Setup uses four equal tabs in both languages:
  Physical Terrain / 物理地形, Climate & Vegetation / 气候植被,
  Hydrology & Regions / 水文区域, and Legacy Modules / 传统模块.
- The preset row, dice action, reset action, seven-axis fingerprint, tab row,
  fixed content viewport, and F5 Generate World footer share one stable shell.
- Every tab owns an independent scroll position. Switching tabs does not reset
  parameters, manual-civilization fields, selection, or scroll state.
- The Legacy Modules tab retains the full pre-existing setup and manual
  civilization workflow instead of replacing or hiding it.

## Parameter Adapter

- One adapter reads and writes the existing map size, ocean, continent
  fragmentation, relief, moisture, drought, vegetation, forest, desert,
  mountain, wetland, natural-region size, and initial-civilization fields.
- New-view and legacy-view edits are bidirectional and immediate. Both Initial
  Civilizations inputs share the same clamped `0..200` value.
- The default Balanced preset uses Extreme map size, 50 for the legacy
  parameter fields, the map-size-aware Medium region value, and leaves the
  initial civilization count unchanged.
- Dice randomizes the existing physical and advanced parameter fields without
  generating a world. Reset restores the approved balanced parameters. F5 and
  the footer use the same existing generation command path.

## Physical Terrain

- The XY plane maps the existing continent-fragmentation value to its horizontal
  axis and ocean amount to its vertical axis, both centered at 50.
- The relief profile has two independent handles. The first writes the existing
  relief value and the second writes the existing mountain-preference value.
  They may cross or occupy the same position.
- Map size remains a direct existing setting and defaults to Extreme for a
  fresh balanced UI state.

## Climate And Vegetation

- Four handles are independently draggable within their assigned quadrants on a
  `-50..+50` plane and may form an irregular quadrilateral.
- Left corners decompose into the existing forest preference, right corners
  into desert preference, upper corners into moisture, and lower corners into
  drought. Integer round trips are deterministic.
- The background labels ice, tundra, temperate grassland, desert, forest,
  monsoon, and tropical rainforest as broad tendencies only. The image is not
  a climate prediction or simulation input.
- Statistical calibration of this explanatory background is intentionally
  deferred to a separate measured-world task.

## Hydrology And Regions

- River Network Density is a visual presentation of the existing wetland
  preference and preserves its `0..100` value.
- Natural-region presets remain map-size dependent:
  Small `5/20/40/60/80`, Medium `10/30/50/70/90`,
  Large `20/40/60/80/100`, and Extreme `30/50/70/85/100`.
- A sixth Custom option accepts any `0..100` value. Entering an exact preset
  value automatically selects that preset.
- Target area and estimated region count continue to use the existing region
  helpers. No region-generation algorithm is duplicated in the UI.

## Presentation Assets And Caching

- Five PNG assets provide the ocean/landmass plane, relief profile, climate
  tendency field, river-density examples, and natural-region scale examples.
- Images contain no localized text or interaction state. Labels, axes, handles,
  selection, hover, press, focus, disabled state, and values are drawn by the
  existing UTF-8 UI path.
- Assets decode through bounded bitmap and surface caches. Stable redraws reuse
  cached surfaces, and a missing asset receives a neutral presentation-only
  fallback.

## Governance

- `AGENTS.md` is now the mandatory concise router.
- Architect, Software Engineer, Validation, UI/UX, Release, Documentation
  Maintainer, and Code Reviewer responsibilities live in separate routed files
  under `agents/`.
- Large cross-ownership initiatives default to isolated `codex/<task>` Git
  worktrees and bounded review phases.
- No agent may stage, commit, tag, merge, push, release, or remove a worktree
  without the exact user authorization required by the repository policy.

## Save And Compatibility

- `WORLD_SIM_VERSION` is `0.3.6.b`.
- `MAP_SAVE_VERSION` remains `20`.
- No save field, block, payload, migration, or rejection policy changes.
- World-generation algorithms, phase order, random streams, map semantics,
  simulation rules, and draw order remain those of Ver0.3.6.a.

## Validation Contract

- Focused coverage includes exact adapter mappings, all region preset
  combinations, custom selection, climate corner limits and round trips,
  crossed/coincident relief handles, fingerprint axes, two-way Initial
  Civilizations synchronization, native-control preservation, shared F5/footer
  generation, asset caching, missing-asset fallback, and bilingual 340/460
  artifacts.
- Final release acceptance additionally requires both build paths, complete
  static/text/source-list gates, performance/resource checks, non-activating
  generated-world GUI evidence, a final diff audit, and a fresh final-source
  Rule39 run as the last gate.

