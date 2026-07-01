# Software Engineer Handoff: Ancient Ocean Decoration Redo

You are Software Engineer for `world_sim_game`.

Repository:
`C:\Users\c4esa\PycharmProjects\world_sim_game`

First rule:
Read and obey `AGENTS.md` before doing anything else. `AGENTS.md` is the source
of truth. If this prompt conflicts with `AGENTS.md`, follow `AGENTS.md`.

Windows command prelude:
Before any git inspection, code search, file search, Python helper command,
build, or validation command, run:

```powershell
$env:PATH='C:\msys64\ucrt64\bin;C:\Users\c4esa\AppData\Local\Programs\Python\Python313;' + $env:PATH
Get-Command gcc
Get-Command python
```

## Current Context

The previous ocean-decoration implementation failed visually. The user rejected
it because:

- the hard rectangular map boundary is still visible,
- the exterior ocean is still flat blue,
- the interior ocean style is effectively unchanged,
- motifs are faint wire icons,
- motifs can hug land/coasts,
- the inside and outside of the map still feel separated.

Positive reference resources are now available in:

`C:\Users\c4esa\PycharmProjects\world_sim_game\assets\ocean_decoration_reference\`

Use these as visual targets:

- `ocean_texture_reference.png`
- `ocean_motif_atlas_reference.png`
- `ocean_integration_target.png`

Use these as direct motif asset sources:

- `motifs/sea_serpent.png`
- `motifs/leviathan_whale.png`
- `motifs/tentacle.png`
- `motifs/sailing_ship.png`
- `motifs/shipwreck.png`
- `motifs/sea_beast.png`
- `motifs/flying_fish.png`
- `motifs/whirlpool.png`
- `motifs/wave_cluster.png`

Use this placement/display metadata:

- `motifs_manifest.tsv`

Use this preview to quickly inspect all direct motif assets:

- `motifs/motifs_contact_sheet.png`

The atlas image is only a style reference. Do not require the implementation to
manually crop motifs from the atlas.

Negative/counterexample screenshots:

- `C:\Users\c4esa\OneDrive\图片\Screenshots\Screenshot 2026-06-29 210405.png`
- `C:\Users\c4esa\OneDrive\图片\Screenshots\Screenshot 2026-06-29 210357.png`

Do not call the work complete if the new output resembles the negative
screenshots.

## Critical Direction

Do not attempt to recreate the approved ocean style with only primitive GDI
wire drawings such as `Arc`, `LineTo`, `MoveToEx`, and `Ellipse`.

That approach already failed.

Implement this as an asset/compositing problem:

1. antique ocean texture,
2. water/exterior masks,
3. substantial motif sprites or sprite-like cached drawings,
4. deterministic placement,
5. cached static composition.

## Likely Current Root Causes

Verify live before editing:

- `src/render/panel_map.c`
  - `draw_map_frame_overlay()` still draws multiple `FrameRect` calls.
- `src/render/render_ocean_decoration.c`
  - exterior cache fills with flat blue.
  - exterior draw excludes `map_rect`, preserving inside/outside separation.
  - interior motif placement only checks a tiny water neighborhood.
- `src/render/render_ocean_motifs.c`
  - motifs are thin primitive line drawings.
- `src/game/game_presentation_map_probe.c`
  - probe checks counts/hash/water-only, not visual quality.

## Ordered Tasks

1. Establish live state:
   - Read `AGENTS.md`.
   - Run `git status --short --branch`, `git log --oneline -5`,
     `git tag --points-at HEAD`.
   - Check whether `world_sim.exe` is running. Do not kill it unless explicitly
     authorized.
   - Confirm root exe inventory.

2. Produce a short function-to-module plan before editing, per AGENTS Rule 16.

3. Rework ocean layering:
   - Exterior ocean must not be a flat blue fill.
   - Interior water must receive the same antique sea texture treatment.
   - The ocean should read as one connected visual field across map interior and
     exterior.
   - Do not preserve a hard inside/outside cut with `ExcludeClipRect(map_rect)`.
   - Keep zoom/pan functional.

4. Remove the hard map frame feeling:
   - Remove or disable the normal gameplay hard map `FrameRect` overlay.
   - Do not leave a gold/brown rectangular frame visible.
   - If an edge cue remains, make it extremely subtle and ocean/paper-like, not
     a UI frame.

5. Add cached antique sea texture:
   - Use `ocean_texture_reference.png` as visual target.
   - Create a cached texture layer with wave hatching, paper grain, mottled wash,
     and visible non-flat water tone.
   - Apply it to exterior ocean and interior water using masks/clipping.
   - Keep it subdued enough that labels, routes, and map data remain readable.
   - Cache/blit the result; do not regenerate expensive texture per frame.

6. Replace motif rendering:
   - Use `ocean_motif_atlas_reference.png` as quality target.
   - Use the individual transparent `motifs/*.png` assets as runtime sources,
     or pack them into a runtime atlas during implementation.
   - Motifs must be substantial illustrated forms, not faint line symbols.
   - Acceptable motifs: sea serpent, whale/leviathan, tentacle/spout, fish-headed
     beast, ship, wreck, flying fish, whirlpool, wave clusters.
   - No repeated compass motif.
   - Prefer sprite/atlas rendering from the transparent PNGs over per-frame
     primitive drawing.

7. Fix motif placement:
   - Read or mirror the suggested default display sizes, footprints, clearance
     values, and weights from `motifs_manifest.tsv`.
   - Use motif footprint plus margin, not only center-tile water checks.
   - Reject interior motifs if footprint plus margin touches or comes too close
     to land/coast, cities, ports, labels, routes, or important icons.
   - Exterior motifs should not hug the old map rectangle edge or UI bars.
   - Fewer well-placed motifs are better than many faint symbols.
   - Placement must be deterministic from map/snapshot seed data.

8. Strengthen probes and artifacts:
   - Extend ocean-decoration probe so success requires:
     - no compass motif,
     - stable deterministic item list across repeated draw,
     - expected cache rebuild behavior only,
     - interior motif footprint clearance from land/coast,
     - exterior and interior texture non-flatness metrics,
     - no hard map-frame overlay in artifact path,
     - artifact screenshots for full map, edge close-up, and zoom/pan state.
   - Probe pass alone is not sufficient; visual artifacts must be inspected.

9. GUI visual validation:
   - Capture screenshots/artifacts showing:
     - full map with exterior ocean,
     - zoomed edge between interior and exterior ocean,
     - interior water texture,
     - motifs in open water away from land,
     - no hard rectangular map frame.
   - Compare against the positive references and the negative screenshots.
   - If output still looks like flat blue with faint line icons, keep fixing.

## Likely Files/Modules

- `src/render/render_ocean_decoration.c/.h`
- `src/render/render_ocean_motifs.c/.h`
- `src/render/render_static_scene.c`
- `src/render/panel_map.c`
- `src/render/render_static_map_cache*.c/.h` only if needed for correct layering
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_probe.c`
- `Makefile`
- `build.bat`
- `assets/ocean_decoration_reference/*` as reference assets

## Non-Goals

- No gameplay changes.
- No simulation changes.
- No save format change.
- No diplomacy, war, alliance, union, collapse, plague, or population changes.
- Do not alter terrain generation, water depth, routes, labels, cities, ports,
  ownership, or map semantics.
- Do not revert unrelated dirty changes.

## Invariants

- Rendering must not mutate simulation state.
- Use `RenderSnapshot` or cached presentation data.
- Keep all `.c/.h` files at or below 500 lines.
- No `.c` includes.
- Add new source files to both `Makefile` and `build.bat`.
- Keep root exe inventory to exactly one `world_sim.exe`.
- If `world_sim.exe` is locked, use AGENTS temporary-target verification and
  delete temp exes.

## Validation Gates

- `make -B world_sim.exe`
- `cmd /c build.bat`
- `git diff --check`
- `make check-text`
- `.c include` scan
- touched-file mojibake scan
- touched/new `.c/.h` line-count audit
- `world_sim.exe --probe-presentation`
- `world_sim.exe --probe-worldgen`
- `world_sim.exe --probe-diplomacy`
- `world_sim.exe --probe-military-alliance`

For this visual task, screenshot/artifact inspection is mandatory. A green probe
with bad visuals is a failure.

## Failure Handling

- If the hard boundary remains, continue fixing.
- If exterior water is flat blue, continue fixing.
- If interior water does not receive matching texture, continue fixing.
- If motifs are still wire icons, continue fixing.
- If motifs touch land or clutter labels/routes, continue fixing.
- If the executable is locked, report the lock and use temporary-target
  verification. Do not kill the process unless explicitly authorized.
- Do not commit, tag, push, or release.

## Final Report Requirements

Report:

- current branch, HEAD, tag, worktree state, process state, exe inventory,
- files changed,
- root causes,
- exact visual changes,
- artifact paths,
- build/check/probe results,
- whether gameplay/save/simulation files changed,
- whether Rule 39 was run,
- no release-ready claim unless AGENTS Rule 39 evidence is complete.
