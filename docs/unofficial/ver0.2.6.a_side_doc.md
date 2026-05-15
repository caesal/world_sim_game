# Ver0.2.6.a Side Doc

Ver0.2.6.a is a patch release over Ver0.2.6 focused on render/UI architecture, natural-region diagnostics, and hydrology rendering polish.

## Main Changes

1. Added stronger AGENTS release and verification rules to prevent missed version/docs/push steps.
2. Continued moving country panels toward snapshot-backed read models so paint can stay responsive with stale-but-visible data.
3. Added compact country card helpers and retained existing country information in more scannable panel sections.
4. Added natural-region sizing and validation helpers for tiny, huge, disconnected, and sliver-shaped regions.
5. Added a dedicated hydrology dirty flag and map-space hydrology cache.
6. Moved river drawing out of the border layer.
7. Added render-side river geometry preparation for smoother visual paths without mutating river generation data.
8. Rendered rivers with hierarchy-aware widths, muted terrain-aware colors, multi-pass strokes, and subtle mouth caps.
9. Added hydrology debug diagnostics for river count, average/longest length, confluences, invalid uphill segments, inland dead ends, short rivers, and cache rebuild timing.

## Validation

- `git diff --check` passed before the Ver0.2.6.a commit.
- `make` passed before the Ver0.2.6.a commit.
