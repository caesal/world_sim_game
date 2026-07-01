# Ocean Decoration Reference Library

Purpose: visual source-of-truth for the ancient ocean decoration redo.

These files are the ocean decoration asset library for the Software Engineer.
The runtime loader consumes `motifs_manifest.tsv` and the individual transparent
PNGs under `motifs/`; preview/reference images remain style guidance only.

## Positive References

- `ocean_texture_reference.png`
  - Target sea surface direction.
  - Muted teal-blue antique nautical-chart water.
  - Visible wave hatching, paper grain, watercolor wash, and non-flat tone.

- `ocean_motif_atlas_reference.png`
  - Target motif quality.
  - Substantial illustrated shapes, ink outlines, wash fills, hatching, and
    recognizable sea creatures/ships.
  - This is the opposite of thin GDI wire icons.
  - This atlas is a style reference, not the runtime asset source.

- `ocean_integration_target.png`
  - Target composition.
  - Interior and exterior ocean should feel like one continuous sea.
  - No hard rectangular map frame.
  - Motifs sit in open water, away from land.

- `motifs/*.png`
  - Individual transparent motif PNGs that can be consumed directly by the
    renderer or converted into a runtime atlas.
  - These are the usable asset sources; do not ask the implementation to crop
    motifs out of `ocean_motif_atlas_reference.png`.

- `motifs_contact_sheet.png`
  - Quick preview of all individual transparent motif PNGs composited over the
    ocean texture.

- `motifs_manifest.tsv`
  - Suggested default display sizes, footprint sizes, clearance values, weights,
    rotation ranges, and placement permissions.

## Negative Examples

The following user screenshots are counterexamples, not targets:

- `C:\Users\c4esa\OneDrive\图片\Screenshots\Screenshot 2026-06-29 210405.png`
- `C:\Users\c4esa\OneDrive\图片\Screenshots\Screenshot 2026-06-29 210357.png`

The failed implementation still had a hard map boundary, flat blue exterior
water, unchanged interior water style, faint wire motifs, and motifs placed too
close to land.

## Implementation Direction

Do not try to reproduce this style only with `Arc`, `LineTo`, `Ellipse`, and
other primitive GDI line drawings. That approach already failed.

Implement the redo as an asset/compositing problem:

1. Use an antique ocean texture layer.
2. Clip/blend it through exterior ocean and interior-water masks.
3. Use the individual transparent motif PNGs in `motifs/`, or convert them into
   an equivalent runtime atlas.
4. Place motifs with footprint-aware land/coast/label/route clearance.
5. Cache the composed layer; do not regenerate or redraw expensive art every
   frame.
