# World Sim Game

This is my first civilization sandbox simulation game.

Goal:
Create a small world map with several civilizations that can expand, form borders, and survive automatically.

## Current Prototype

Ver0.1.3 is a Windows graphical sandbox prototype written in C.

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
19. Generate cold regions from random climate centers instead of fixed top/bottom poles
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
12. Use the right-side tabs to switch between info, civilization controls, and map generation
13. Drag the panel divider to resize the side controls
14. Mouse wheel zooms the map around the cursor
15. In the Map tab, click mode buttons to switch map layers
16. In the Map tab, drag generation sliders to adjust the next generated world
17. Hold right mouse button and drag the map to pan
18. Hover over compact stat blocks in the right panel to see their meaning

## Build

Install a C compiler such as MSYS2 MinGW GCC, then run this in the MSYS2 UCRT64 terminal:

```bash
gcc -O2 src/main.c src/core/game_state.c src/world/world.c src/gui/render.c src/gui/ui.c -o world_sim.exe -lgdi32 -lmsimg32 -mwindows
./world_sim.exe
```

If you are using PowerShell, add the MSYS2 compiler folder for the current terminal session first:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
gcc -O2 src\main.c src\core\game_state.c src\world\world.c src\gui\render.c src\gui\ui.c -o world_sim.exe -lgdi32 -lmsimg32 -mwindows
.\world_sim.exe
```

## Source Layout

1. `src/main.c` starts the program
2. `src/core` contains shared types, constants, state, helpers, and the version marker
3. `src/world` contains world generation, geography and climate logic, cities, provinces, and simulation rules
4. `src/gui` contains Win32 input handling, controls, map rendering, panels, and icon drawing
5. `src/game.h` provides the top-level game entry include used by `main.c`
6. `assets/icons` contains the PNG icons used by the right-side information panel
