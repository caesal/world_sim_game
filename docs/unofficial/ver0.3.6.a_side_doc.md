# Ver0.3.6.a Side Doc

Ver0.3.6.a is the static geography, climate, hydrology, and immutable map
presentation release over Ver0.3.6. The named global plague model and Plague
panel from Ver0.3.6 remain intact.

This release intentionally does not update `docs/official`; that directory is
the separately requested official documentation-freeze series. The release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Physical World

- World generation now prepares and validates a complete off-world physical
  state before atomically publishing it.
- Elevation uses coherent land/ocean selection, broad mountains, shoulders,
  cores, branches, saddles, highlands, basins, and coastal transitions.
- A deterministic annual wind field uses 16 directions and speed 0-100.
  Geography and Climate views show uniform-width arrows whose heading is wind
  direction and whose length is wind speed.
- Fixed-point moisture transport includes ocean recharge, lateral mixing,
  orographic precipitation, lee drying, and rain-shadow recovery.
- Terrain classification consumes finalized elevation, temperature, moisture,
  hydroclimate, and generation preferences. Geography is not recomputed by the
  later civilization simulation.

## Hydrology And Lakes

- Drainage uses deterministic eight-neighbor routing, priority-flood elevation,
  receiver graphs, flow accumulation, Strahler order, confluences, main stems,
  mouths, deltas, and distributaries.
- Standing-water lakes require coherent four-connected shapes, depth, core,
  catchment, outlet, compactness, and topology support. Ordinary depressions no
  longer become false mesh-like lakes.
- Rejected lake candidates retain their real underlying plain, basin, wetland,
  hill, plateau, oasis, coast, or delta geography.
- Inland lakes reuse the existing water texture language with the approved
  lighter blue-white transform and a matching legend swatch. Ocean decoration
  remains ocean-only.
- Rivers use one blue body without a dark outline. Their geometry preserves
  shared source, confluence, lake-outlet, mouth, delta, and distributary
  anchors.

## Coast And Low-Ocean Worlds

- The former threshold restoration used globally distributed tie selection and
  could create diagonal lattices, combs, and sparse coastal meshes. Selection
  now grows or shrinks a deterministic four-neighbor frontier using topology,
  compactness, diagonal support, continuous subheight, and a stable final tie.
- The land target is
  `round(tile_count * (10000 - 72 * ocean_amount) / 10000)`, preserving the
  established Ocean Amount behavior while avoiding integer-percent plateaus.
- Threshold land receives the minimum positive land elevation and a transient
  coastal-lowland hint. Final precedence is Delta, Wetland, Oasis, Coast, then
  Plain according to hydrology and climate.
- River paths use exact-sized, validated, transactional ownership across world
  generation, commit, save/load, RenderSnapshot, rollback, and cleanup. A
  128 MiB payload ceiling rejects malformed counts without truncating valid
  generated rivers.
- Genuine precommit generation failures keep the previous world and publish a
  localized stage-specific explanation. Snapshot or prewarm recovery does not
  regenerate the committed physical world.

## Immutable Presentation

- Physical terrain/coast, water, rivers, and wind are revisioned immutable
  assets prepared after generation or load.
- River views target 18, 40, 75, and 100 percent of generated paths at
  100/150/225/300 percent zoom. Selection is by semantic stem and includes its
  downstream closure.
- Camera pan, zoom, and Geography/Climate switching select, crop, scale, and
  compose prebuilt assets. They do not rescan tiles or paths or rerasterize the
  static world after prewarm.
- Political ownership, province fill, borders, city icons, labels, routes,
  selections, highlights, announcements, and plague fog remain realtime layers
  and keep their established draw order.

## Save And Compatibility

- `WORLD_SIM_VERSION` is `0.3.6.a`.
- `MAP_SAVE_VERSION` is `20`.
- The PHY20 payload persists dimensions, immutable per-tile physical fields,
  rivers, wind, fertility, counts, item sizes, and checksum-protected data.
- Save versions 19 and earlier are rejected before incompatible payload use.
  No migration is attempted.

## Validation Evidence

- Canonical Make and batch builds passed from the same frozen source.
- Worldgen, presentation, save, plague model, plague baseline, and 1,000-city
  stress probes passed.
- Extreme Ocean Amount `0/10/20/30/40/44/45/46/47/50` and twenty randomized
  Large/Extreme generations completed without target drift, retry, truncation,
  or blank-map fallback.
- Deterministic river fixtures verified downstream continuity and full 300
  percent networks. Targeted GUI evidence covered Geography, Climate, and
  Political modes at all four river LODs.
- After prewarm, matched zoom/mode runs recorded zero immutable rebuilds,
  static tile/path/wind scans, new full-client immutable allocations, or
  retained GDI/private/working-set growth.
- Final Rule39 reached Year 715 Month 9 on an Extreme 1152x800 world with 26
  initial civilizations and 1,211 natural regions at max/5x. The first five
  stage-5 civilizations were `0 Hagi`, `1 Takunmara`, `2 Gunthertor`,
  `3 Ottoland`, and `4 Kalemba`; deep routes moved from hidden `0/0/0` to
  visible `16/14/2` total/shallow/deep.
- Physical, wind, and river identities remained stable while political fill,
  borders, cities, labels, routes, and selected extents stayed current. No
  presentation month was dropped, no ordering skip occurred, and both 60-frame
  checks reported zero anomalies.

## Residual Risk

- The immutable presentation cache deliberately retains substantial bounded
  bitmap memory on Extreme maps. Repeated camera/mode cycles showed no growth,
  but the fixed footprint remains a cost of avoiding rerasterization.
- Individual wheel or settle peaks vary with the machine and generated world;
  the hard acceptance evidence is zero post-prewarm immutable rebuild/scan/
  allocation growth plus matched task-level performance.
- Older save formats remain deliberately incompatible.
