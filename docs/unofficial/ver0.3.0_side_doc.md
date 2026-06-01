# Ver0.3.0 Side Doc

Ver0.3.0 is a render-only marker and sea-lane stability checkpoint over
Ver0.2.12.d. It preserves the successful max-speed responsiveness work while
fixing the visible city/harbor marker regression and stabilizing route visual
offsets.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.0`.
2. Replaced the overly crude settlement marker glyphs with clearer lightweight
   outpost, village, town, and city silhouettes.
3. Replaced the crude harbor marker with a distinct lightweight anchor-style
   glyph.
4. Preserved capital rings, harbor-capital rings, neutral visibility rules,
   one-marker port behavior, and marker counters.
5. Stabilized sea-lane visual identities so route offsets no longer depend on
   transient `lanes_revision`, lane index, or route-potential edge index.
6. Derived route visual keys from stable route geometry inputs, including route
   type, regions, endpoint ports, map size, point count, and sampled path
   points.

## Behavioral Notes

- This is a render-only cleanup. It is not a gameplay, simulation, worldgen,
  diplomacy, war, plague, population, technology, or balance checkpoint.
- City and harbor markers remain lightweight and avoid the old per-marker
  PNG/GDI+ draw path.
- Sea-lane route graph data and deep-sea unlock rules are intended to remain
  unchanged.
- The Ver0.2.12.d stutter, year-jump, stale-snapshot, and UI responsiveness
  improvements are intended to remain intact.

## Validation

- `WORLD_SIM_VERSION` is `0.3.0`.
- `MAP_SAVE_VERSION` remains 10.
- Build/static validation passed for the render-only cleanup.
- Focused screenshots showed recognizable city/harbor markers, route-potential
  lines, and repeated same-view route captures without visible side-flip or
  route twist.
- Strict AGENTS regression was reported complete with:
  - Large map.
  - 26 initial civilizations.
  - 748 natural regions.
  - 748 cities.
  - Max speed / index 4, target 0.1s/month.
  - Final observed time: Year 505 Month 1.
  - Five distinct civilizations at technology stage 5 or beyond:
    civ 88 / League of Silvercoast, civ 48 / Stormborn Dominion,
    civ 168 / The Sunward Empire, civ 188 / Great SoraCheong Mountain Realm,
    and civ 78 / The Dragon Throne.
  - Deep-sea evidence before and after unlock, including final active lanes
    100, shallow 67, and deep 33.
