# Ver0.2.6 Side Doc

Ver0.2.6 focuses on making route visuals, world-generation feedback, province names, and diplomacy contact rules match what the player can actually see on the map.

## Main Changes

1. World generation now separates overall progress from current-stage progress.
2. The generation mask hides half-built map layers while keeping the right sidebar visible.
3. Province and natural-region labels use stable bilingual name IDs from the province-name table.
4. The ordinary map now defaults to the political layer instead of the old mixed All view.
5. Sea-lane visuals use clearer shallow/deep colors, stable world-distance dash rhythm, and adjusted line widths.
6. Deep route potential is kept as a sparse backbone connecting shallow networks.
7. Diplomacy contact now uses one current-contact source for land borders, active shallow sea networks, active deep sea networks, and vassal proxy contact.
8. Peace, tension, war starts, and diplomacy map animations no longer occur when there is no current land or active sea-lane contact.
9. Disconnected known relations fade instead of escalating into tension or war.
10. War front checks use the same contact source as diplomacy.

## Validation

- `git diff --check` passed before the Ver0.2.6 commit.
- `make TARGET=world_sim_check.exe` passed because the normal `world_sim.exe` output was locked by a running game process.
- The temporary verification executable was removed after the build check.
