# World Sim Game

This is my first civilization sandbox simulation game.

Goal:
Create a small world map with several civilizations that can expand, fight, and survive automatically.

## Current Prototype

Ver0.1.0 is a terminal sandbox prototype written in C.

You can:

1. Generate a random world map with oceans, coast, grassland, forest, desert, and mountains
2. Create civilizations and place them on empty land
3. Set civilization preferences: aggression, expansion, defense, and culture
4. Simulate days and watch countries expand, fight, grow, or fall
5. Inspect map tiles and view civilization stats

## Build

Install a C compiler such as MinGW GCC or Visual Studio Build Tools, then run:

```powershell
gcc src/main.c -o world_sim.exe
```

Run the game:

```powershell
.\world_sim.exe
```
