# World Sim Game

This is my first civilization sandbox simulation game.

Goal:
Create a small world map with several civilizations that can expand, fight, and survive automatically.

## Current Prototype

Ver0.1.2 is a Windows graphical sandbox prototype written in C.

You can:

1. Generate a random 1920x1080 visual world map from continuous elevation, moisture, and temperature fields
2. Watch civilizations expand as colored territory on a higher-detail visual map
3. See country borders, coast outlines, cities, and year/month progress
4. Simulate month by month with three auto-run speeds
5. Click map tiles to inspect terrain, ownership, resources, local combat modifiers, and administrative region totals
6. Add or edit civilizations from the right-side form while the simulation is running
7. Let cities control nearby regions, with city range shaped by population and local resources
8. Rebuild worlds with a land/ocean ratio slider and initial civilization count
9. Drag the left edge of the right-side panel to resize the controls area
10. Use mouse wheel zoom centered on the cursor
11. See rivers, hills, mountains, and more geographically coherent terrain zones

## Controls

1. `Space` starts or pauses auto-run
2. The bottom play button also starts or pauses auto-run
3. `1` or the first speed button sets slow speed, 2 seconds per month
4. `2` or the second speed button sets normal speed, 0.5 seconds per month
5. `3` or the third speed button sets fast speed, 0.1 seconds per month
6. `F1` adds a civilization from the right-side form
7. `F2` applies the form to the selected civilization
8. `F5` generates a new random world using the right-side world setup
9. `R` also generates a new random world when the map has keyboard focus
10. `Esc` quits the game
11. Left mouse click selects a tile or civilization
12. Drag the Land/Ocean slider to adjust the next generated world
13. Drag the panel divider to resize the side controls
14. Mouse wheel zooms the map around the cursor

## Build

Install a C compiler such as MSYS2 MinGW GCC, then run this in the MSYS2 UCRT64 terminal:

```bash
gcc src/main.c src/game.c -o world_sim.exe -lgdi32 -mwindows
./world_sim.exe
```

If you are using PowerShell, add the MSYS2 compiler folder for the current terminal session first:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
gcc src\main.c src\game.c -o world_sim.exe -lgdi32 -mwindows
.\world_sim.exe
```
