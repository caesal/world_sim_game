# World Generation UI Assets

Generated with OpenAI `imagegen` from the five approved UI reference crops on
2026-07-21. These PNG files are presentation assets only. They must not change
world-generation behavior or become simulation inputs.

## Asset Manifest

| File | Source size | Intended use | SHA-256 |
|---|---:|---|---|
| `landmass_ocean_xy.png` | 1254x1254 | Physical XY plane background | `A899BF93CCD02C6CEEF91E42D1149280D7009D300DD152649B49D743D5CADB31` |
| `relief_profile.png` | 2048x768 | Elevation-range profile background | `606B5ED532612A0CE2D93AC9B189B20C928D34D6F2A6AC625EBE952100764596` |
| `climate_biome_envelope.png` | 1536x1024 | Temperature/humidity tendency background | `9AD0368E91D26C132D8DB7DF3CBDD7F939ACD8A1A192DF8E01869A8C059F7927` |
| `river_density_atlas.png` | 2172x724 | Five river-density examples | `D2DCE9CA704C65D2D54301B92238B711169E22B821E58CD96F2945CB7B988535` |
| `natural_region_scale_atlas.png` | 2048x768 | Five natural-region-scale examples | `F67217DC3263478090DF64923871FAAB68E63C3BD15399BC2F513BB02406CB0F` |

## Integration Contract

- Draw all localized labels, values, axes, crosshairs, ticks, handles, focus,
  hover, disabled, and selected states in the existing UI renderer. None of
  those states are baked into these images.
- Decode each PNG once and retain a bounded presentation cache. Never perform
  file I/O, PNG decoding, or source-image resampling every frame.
- Keep image scaling and clipping inside the panel presentation layer. Do not
  include these assets in static map caches or simulation snapshots.
- Preserve the existing world-generation configuration interface. The images
  are visual explanations of the controls, not lookup tables.
- Maintain aspect ratio. Crop only where specified below; do not stretch.

## Layout Mapping

### Physical XY Plane

Use `landmass_ocean_xy.png` as the square plot background. Overlay the axes at
50/50.

- X axis: large/coherent landmasses on the left, fragmented/small landmasses
  on the right.
- Y axis: low ocean at the bottom, high ocean at the top.
- The four visual zones already follow this orientation.

### Relief Range

Use `relief_profile.png` as the wide profile behind the two-pin range control.
Place the minimum and maximum pins on the common baseline. The image progresses
from low relief on the left to high relief on the right.

### Climate Envelope

Use `climate_biome_envelope.png` behind the four-corner tendency envelope.

- X axis: cold on the left, hot on the right.
- Y axis: dry at the bottom, humid at the top.
- Display both axes as `-50..+50`, crossing at zero.
- Draw the seven localized biome labels in code: ice, tundra, temperate
  grassland, desert, forest, monsoon, and tropical rainforest.
- The colored zones are explanatory tendencies only, not predictions or
  generator lookup data.

### River Density

Use `river_density_atlas.png` as five equal conceptual horizontal slots, from
sparse on the left to extremely dense on the right. Draw the radio/selection
controls and localized captions separately. If exact slot crops are needed,
divide the source width into five equal normalized regions and inset each crop
slightly to retain the dark gutter.

### Natural Region Scale

Use `natural_region_scale_atlas.png` as five equal horizontal slots, from many
tiny regions on the left to a few very large regions on the right. Draw tile
frames, selection outlines, and localized captions separately. Divide the
source width into five equal normalized regions for individual tile crops.

## Generation Prompts

The prompts intentionally excluded UI chrome so the same assets can be used in
English and Chinese layouts.

### Landmass/Ocean

Create a clean square orthographic top-down map preview with four conceptual
quadrants: high-ocean coherent continents, high-ocean archipelago, low-ocean
supercontinent, and low-ocean fragmented islands. Use dark desaturated
blue-teal water and natural olive/moss land with subtle strategy-map texture.
No text, axes, crosshair, frame, handles, political colors, or UI chrome.

### Relief

Create one wide continuous terrain cross-section progressing from grassy plain
and foothills to layered ridges and tall snowy alpine peaks. Use a uniform dark
charcoal-teal background and a straight baseline. No labels, ticks, frame,
handles, sky elements, or UI chrome.

### Climate

Create a rectangular abstract biome field for a cold-to-hot horizontal axis and
a dry-to-wet vertical axis. Place icy blue-white at lower-left, blue-gray tundra
at upper-left, pale sage grassland near center, warm ochre desert at
lower-right, olive forest at upper-center, jade monsoon at middle-right, and
deep teal rainforest at upper-right. Use broad, softly overlapping regions and
keep contrast low enough for white control overlays. No labels, grid, axes,
frame, or handles.

### River Density

Create exactly five isolated blue drainage-network diagrams on a dark
charcoal-teal background, progressing from one sparse main river to an extremely
dense dendritic basin. Keep a consistent line family and separate gutters. No
labels, buttons, selection states, frames, or land shapes.

### Natural Region Scale

Create exactly five equal diagram areas containing organic Voronoi-like region
networks in muted antique-gold lines, progressing from many tiny cells to a few
very large cells. Use a dark charcoal-teal background. No labels, selection
state, purple accents, buttons, or UI chrome.
