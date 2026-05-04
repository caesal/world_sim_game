# World Sim Game

This is my first civilization sandbox simulation game.

Goal:
Create a small world map with several civilizations that can expand, form borders, and survive automatically.

## Current Prototype

Ver0.1.6 is a Windows graphical sandbox prototype written in C.

You can:

1. Generate a random 1920x1080 visual world map from continuous elevation, moisture, and temperature fields
2. Watch civilizations expand as colored territory on a higher-detail visual map
3. See country borders, coast outlines, cities, and year/month progress
4. Simulate month by month with three auto-run speeds
5. Click map tiles to inspect terrain, ownership, resources, local modifiers, and administrative region totals
6. Add or edit civilizations from the right-side form while the simulation is running
7. Let cities create fixed province shapes that stay stable after they are established
8. Rebuild worlds with a land/ocean ratio slider and initial civilization count
9. Drag the left edge of the right-side panel to resize the controls area, with a reserved minimum width for readable stat blocks
10. Use mouse wheel zoom centered on the cursor
11. See rivers, hills, mountains, and more geographically coherent terrain zones
12. Switch between all-map, climate, geography, and political display modes
13. View separate geography and climate information for the selected tile
14. See thin province borders inside thick country borders
15. Let civilizations form stable borders when they contact each other instead of fighting
16. Keep cities farther apart so provinces do not cluster into tiny rings
17. Use right-side tabs for selected info, civilization management, and map generation
18. Tune ocean, mountain, desert, forest, and wetland generation from the map tab
19. Generate cold regions from a randomized latitude axis instead of fixed top/bottom poles
20. Use a higher-density 800x600 internal map grid with crisp tile rendering
21. Drag the map with the right mouse button
22. Keep peaceful contact borders stable while civilizations expand toward open land
23. Show country, tile, combat, and province resources as compact image-icon metric blocks with hover labels
24. Give newly founded cities and provinces generated names
25. Vary same-terrain colors by elevation so hills, mountains, and plains read with more local detail
26. Track livestock as a separate local resource alongside food, wood, ore, and water
27. Cache province ownership and province summaries so right-panel inspection and border drawing stay responsive
28. Render only visible map details while panning or zooming
29. Draw the high-density map through a crisp pixel surface instead of one rectangle per tile
30. Use dedicated icons for territory, disorder, migration, and economy in the UI icon registry
31. Generate maps from reusable fractal value-noise fields so terrain regions are less speckled
32. Show country population, disorder factors, province resources, and a collapsible transparent map legend in the UI
33. Keep diplomacy and war code in dedicated simulation modules for upcoming alliance, vassal, win, and loss rules
34. Keep the right panel wide enough for four metric blocks per row
35. Draw PNG icons directly through GDI+ so their transparent backgrounds stay transparent
36. Track geography, climate, ecology, and resource features as separate map layers
37. Let province shapes follow geography-aware growth costs instead of fixed circular regions
38. Derive climate from elevation, distance to sea, latitude, and mountain rain shadow
39. Use the new crisp civilization icon package for resource and metric blocks
40. Use the matching-style icon package for covered map, city, combat, territory, and disorder icons
41. Use a dedicated Diplomacy tab to inspect contacted civilizations, relation factors, diplomatic status, and war progress
42. See selected-country military strength, capital garrison estimates, and province garrison estimates in the Diplomacy tab
43. Keep the year/month top bar visible above the map and reduce white repaint flashes during tab or panel interaction
44. Use city stage icons for outpost, village, town, city, capital, and harbor markers
45. Show Chinese two-character stat labels beside icons when the UI language is Chinese
46. Keep versioned design PDFs in `docs` with version-matched filenames
47. Keep `.c` and `.h` files under the 500-line module size rule
48. Draw smoother cartographic country borders, province borders, coastlines, political fills, labels, and subtle map grid overlays
49. Render continuous river path objects instead of scattered tile-center fragments
50. Restyle city, capital, harbor, hill, and mountain map markers toward an old political map look while reusing existing assets
51. Rebalance diplomacy so prosperous neighboring civilizations can stabilize relations through trade fit instead of drifting into tension only because they are self-sufficient
52. Add explicit maritime route paths for port-to-port contact, migration, diplomacy exposure, overseas expansion, and dashed sea-lane rendering
53. Add city-level age/sex population cohorts, derived country population summaries, soldier casualty population loss, and an Info-tab population pyramid
54. Replace flat plague events with persistent city outbreaks, percentage deaths, spread pressure, disorder impact, and dark green map visualization
55. Cache expensive map render layers across ordinary repaints and avoid duplicate maritime route rebuilds during monthly simulation
56. Cache diplomacy border contacts and population summaries so monthly updates do less repeated full-map aggregation
57. Split core shared types into narrower `constants.h`, `world_types.h`, and `sim_types.h` headers while keeping `game_types.h` as the compatibility entry point
58. Document the current map-rendering and river-polish diagnostic for the next targeted cleanup pass

## Controls

1. `Space` starts or pauses auto-run
2. The bottom play button also starts or pauses auto-run
3. The first speed button sets slow speed, 1 second per month
4. The second speed button sets normal speed, 0.25 seconds per month
5. The third speed button sets fast speed, 0.05 seconds per month
6. `F1` adds a civilization from the right-side form
7. `F2` applies the form to the selected civilization
8. `F5` generates a new random world using the right-side world setup
9. `R` also generates a new random world when the map has keyboard focus
10. `Esc` quits the game
11. Left mouse click selects a tile or civilization
12. Use the right-side tabs to switch between info, civilization controls, diplomacy, and map generation
13. Drag the panel divider to resize the side controls
14. Mouse wheel zooms the map around the cursor
15. In the Map tab, click mode buttons to switch map layers
16. In the Map tab, drag generation sliders to adjust the next generated world
17. Hold right mouse button and drag the map to pan
18. Hover over compact stat blocks in the right panel to see their meaning

## Build

Install a C compiler such as MSYS2 MinGW GCC. If `make` is installed, run:

```bash
make
./world_sim.exe
```

On Windows, `build.bat` uses the same source list:

```bat
build.bat
world_sim.exe
```

The full source list changes as modules are split, so `Makefile` and `build.bat` are the canonical build commands.
If you are using PowerShell, add the MSYS2 compiler folder for the current terminal session first:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
.\build.bat
.\world_sim.exe
```

## Source Layout

1. `src/main.c` starts the program
2. `src/game/game.c` and `src/game/game.h` own game startup and the main message loop
3. `src/core/game_types.h` contains shared extern state and compatibility includes for shared game types
4. `src/core/constants.h`, `src/core/world_types.h`, and `src/core/sim_types.h` split shared constants, world structs, and simulation structs by responsibility
5. `src/core` contains shared state definitions, common helper implementations, and the version marker
6. `src/world/world_gen.c` and `src/world/world_gen.h` contain top-level world generation
7. `src/sim/simulation.c` and `src/sim/simulation.h` contain civilization seeding, summaries, and month/year simulation coordination
8. `src/render` contains drawing-only map, panel, border, icon, route, plague, label, and legend rendering split by responsibility
9. `src/ui` contains Win32 input, form controls, buttons, sliders, and UI layout helpers
10. `src/data` contains editable geography, climate, ecology, resource, and civilization metric tables
11. `src/world` contains support modules for generation, smoothing, noise, rivers, ports, and terrain queries
12. `src/sim` contains civilization metrics, province logic, population, plague, ports, maritime, diplomacy, expansion, and war simulation submodules
13. `assets/icons` contains the PNG icons used by the right-side information panel
14. `Makefile` contains the canonical build command
15. `build.bat` mirrors the same source list for Windows command prompts
16. `docs/ver0.1.5_province_expansion_logic.pdf` and `docs/ver0.1.5_diplomacy_war_framework.pdf` track the design references used by the current simulation systems
