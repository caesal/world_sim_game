# Version Log

## Ver0.1.5

Implemented features:

1. Bumped the active prototype version to Ver0.1.5
2. Added a dedicated right-side Diplomacy tab separate from the civilization editing tab
3. Diplomacy now lists only civilizations that have been contacted by the selected country
4. Each contacted relationship shows relation score, status, border tension, trade fit, resource conflict, border length, natural barrier, years known, and truce years
5. War relationships display a two-color progress bar using the selected country on the left and the opponent on the right
6. The war progress bar includes current soldiers, losses, and battle wins for both sides
7. Added read-only war soldier accessors so rendering can show military state without mutating simulation data
8. The Diplomacy tab shows available soldiers, mobilization base, capital garrison estimate, and province-level garrison estimates
9. Province military values are displayed as estimated garrisons because the current simulation does not yet store persistent per-province armies
10. Updated the right-side tab layout so Info, Civ, Diplomacy, and Map are all reachable
11. Updated README current-version notes for the Ver0.1.5 diplomacy UI
12. Kept the year/month top bar above the map draw pass so it is not covered by the map surface
13. Suppressed background erasing to reduce white flashes while switching tabs or resizing the side panel
14. Replaced city markers with the matching-style outpost, village, town, city, capital, and harbor icons
15. Replaced diplomacy factor text rows with icon metric blocks and hover labels
16. Localized compact metric labels to Chinese two-character labels in Chinese mode
17. Moved source files into responsibility folders: `core`, `game`, `world`, `sim`, `render`, and `ui`
18. Split `render.c` into map, panel, diplomacy, shared render helper, and icon modules
19. Split river generation into `src/world/rivers.c` and `src/world/rivers.h`
20. Added the 500-line `.c` and `.h` rule to `AGENTS.md` and verified the source tree follows it
21. Renamed design PDFs to versioned filenames: `ver0.1.5_province_expansion_logic.pdf`, `ver0.1.5_diplomacy_war_framework.pdf`, and `ver0.1.4a_code_review_notes_for_codex.pdf`
22. Updated `Makefile`, `build.bat`, README build commands, and source layout docs for the categorized tree
23. Reduced short scattered river fragments by moving river carving into a dedicated river-generation pass with minimum length and spacing checks

## Ver0.1.4.a

Freeze notes:

1. Replaced covered UI icons with the matching-style icon package while keeping existing resource icons for resource types not included in the new package
2. Added root `Makefile` using the current module list
3. Added the province expansion and diplomacy/war framework PDFs to `docs`
4. Tightened module includes so world generation no longer depends on the port or simulation boundary
5. Confirmed the first-stage refactor structure and kept `AGENTS.md` in place for future agent rules

## Ver0.1.4

Implemented features:

1. Bumped the active prototype version to Ver0.1.4
2. Fixed default civilization seeding beyond eight civilizations by expanding names, symbols, and trait presets to the full civilization limit
3. Restored province borders with a separate alpha border layer for smoother internal province and country outlines
4. Rebuilt the selected information panel around country and province sections instead of bold generic labels
5. Restored country population, land, city count, disorder, and province resource metrics
6. Added visible factor hints and hover labels for aggregate values such as population, disorder, and habitability
7. Added a bottom-right map legend for geography and climate colors
8. Strengthened the desert slider so dry climate generation changes more clearly
9. Changed world generation to reusable fractal value-noise fields for larger, more coherent land, moisture, and temperature regions
10. Made Ocean 0 produce land-first maps instead of shallow-water noise
11. Added `src/world/noise.c` and `src/world/noise.h` for map noise generation
12. Added `src/sim/diplomacy.c`, `src/sim/diplomacy.h`, `src/sim/war.c`, and `src/sim/war.h` as the first diplomacy and war structure
13. Added first-pass diplomacy contact tracking when living civilizations touch borders
14. Updated build documentation for the new module layout
15. Widened the default and minimum side panel so four metric blocks fit without squeezing
16. Switched icon rendering to draw PNGs through GDI+ directly so transparent icons keep their alpha channel
17. Expanded climate categories to tropical rainforest, monsoon, savanna, desert, semi-arid, mediterranean, oceanic, temperate monsoon, continental, subarctic, tundra, ice cap, alpine, and highland plateau
18. Expanded geography categories to the current design set: ocean, coast, plain, hill, mountain, plateau, basin, canyon, volcano, lake, bay, delta, wetland, oasis, and island
19. Added separate ecology and resource layers so a tile can have geography, climate, ecology, and resource features at the same time
20. Made province shapes grow with geography-aware frontier costs instead of simple circular city radii
21. Added ecology and resource names to selected province inspection
22. Smoothed province and country outlines through a higher-resolution border alpha surface
23. Added `src/data/game_tables.h` and `src/data/game_tables.c` for editable geography, climate, ecology, resource, and civilization metric tables
24. Pruned unsupported geography entries from generation tables so the code follows the current design charts more strictly
25. Added first-pass language switching for the main UI text path and selected-info panel labels
26. Started replacing old A/E/D/C traits with governance, cohesion, production, military, commerce, logistics, and innovation
27. Replaced the old four terrain sliders with world controls for ocean, continent fragmentation, relief, moisture, drought, vegetation, and advanced terrain bias
28. Connected every world-generation slider to the fractal map generator so rebuilding the world reflects the selected values
29. Added dynamic adaptation as a changing civilization state derived from environment, resources, culture, and disorder instead of a fixed core metric
30. Tightened the geography list to the approved table and kept Island as the only island category
31. Added easy-edit text reference tables to `src/data/game_tables.h` for the map pipeline, geography, climate, resources, and civilization metrics
32. Updated expansion so nearby claims attach to an existing city province while distant high-value targets can create frontier cities and provinces
33. Added first-pass alliance and vassal diplomacy states, with vassal outcomes available after severe war defeats

## Ver0.1.3

Implemented features:

1. Bumped the active prototype version to Ver0.1.3
2. Made right-panel stat labels true hover tooltips instead of persistent bottom overlays
3. Kept fixed province shapes after a city establishes its administrative region
4. Stopped city population growth from reshaping existing province borders
5. Added fixed province ids to map tiles so province ownership and province shape are separate concepts
6. Disabled active conquest during expansion for this prototype pass
7. Changed civilization expansion so contacted civilizations form borders and expand toward unowned land instead
8. Made newly founded cities immediately lock in their province region
9. Prevented new cities and starting civilizations from being placed inside an existing province shape
10. Reorganized source files into `src/core`, `src/world`, and `src/gui`
11. Added a shared `version.h` marker for Ver0.1.3

## Ver0.1.2

Implemented features:

1. Larger 1920x1080 graphical window
2. Higher-detail map grid
3. More random world seeding between launches
4. Month-based simulation
5. Space toggles pause and run
6. Three speeds
7. Right-side form for adding civilizations during simulation
8. Right-side form for editing selected civilization attributes
9. City system with capital cities and resource-based control radius
10. City capture transfers the controlled region
11. New terrain types: icefield and wetland
12. Terrain resource values and local attack/defense modifiers
13. Keyboard shortcuts for form actions: F1 add civilization, F2 apply selected, F5 new world
14. Land/ocean ratio slider for new worlds
15. Initial civilization count option
16. Draggable side panel divider
17. More reliable continent generation when rebuilding the world
18. Bottom play/pause and speed buttons
19. Better global shortcut handling when form controls have focus
20. Speeds changed to 1, 0.25, and 0.05 seconds per month
21. Mouse wheel zoom centered on the cursor
22. Height, moisture, and temperature based map generation
23. More coherent coastlines and terrain regions
24. Hills and rivers
25. Tile-level resource variation shown as numbers
26. Administrative region summaries when clicking city-controlled land
27. Split the code into categorized modules: `game_state.c`, `world.c`, `render.c`, and `ui.c`
28. Removed `1`, `2`, and `3` as keyboard speed shortcuts so numeric input fields can accept numbers
29. Added separate geography and climate layers with mode buttons
30. Added all-map, climate, geography, and political display modes
31. Added thin province borders alongside thick country borders
32. Added first-pass capital fall handling with relocation or collapse based on stability
33. Replaced the unity wrapper with real `.c/.h` module pairs for shared state, world logic, rendering, and UI
34. Smoothed the rendered map surface with interpolated colors instead of enlarged tile rectangles
35. Reduced repetitive tile texture marks that made the map read like a grid
36. Changed province display to use nearest city ownership inside each country, avoiding circular city-radius borders
37. Added a minimum city distance so cities and provinces spread out more naturally
38. Added right-side tabs for selected info, civilization management, and map generation
39. Moved map view mode buttons into the Map tab
40. Added generation sliders for ocean, mountains, desert, forest, and wetland
41. Changed cold climate generation to use random cold centers instead of fixed north/south poles
42. Increased the default map zoom so the world reads larger on first open
43. Increased the map grid to 240x135 for a clearer world without blurred interpolation
44. Restored crisp tile rendering instead of smoothed color interpolation
45. Added right mouse drag panning for moving around the map
46. Fixed collapsed countries leaving zombie territory after their capital falls
47. Made capital/province capture trigger when any tile in the province is successfully conquered
48. Added compact image-icon stat blocks with hover labels for country, tile, combat, and province resources
49. Added generated city/province names
50. Added elevation-based color variation so same geography can still show local differences
51. Removed the old Overview map mode button and made All the first map view
52. Widened right-panel stat blocks so icons and values do not overlap
53. Added the provided PNG icon set as project assets for population, combat, expansion, defense, culture, geography, climate, habitability, and resources
54. Added livestock as a separate terrain and province resource
55. Reserved a wider minimum side panel width so stat blocks and controls do not squeeze together when resized
56. Cached province ownership and province summaries to avoid repeated city scans during rendering and tile inspection
57. Culled off-screen map rendering so panning and zooming only draw visible tiles, rivers, and borders
58. Combined monthly resource scoring into one map scan for all civilizations instead of one full scan per civilization
59. Increased the internal map grid to 800x600 landscape tiles
60. Replaced per-tile base rendering with a crisp bitmap surface for better performance on high-density maps
61. Updated build instructions to use `-O2` optimization
62. Added the new territory, disorder, migration, and economy PNG icons to the UI icon registry
63. Moved the high-density smoothing buffer out of the stack to avoid large-map stack pressure
64. Rebalanced the ocean slider so Ocean 0 creates mostly land instead of dense shallow-water noise
65. Enlarged high-density terrain features so generated maps read less speckled
66. Added editable geography, climate, ecology, resource, and civilization metric tables in `src/data`
67. Pruned unsupported geography entries so generation follows the current design table only
68. Added first-pass language switching for English and Chinese UI labels
69. Added the seven civilization metrics: governance, cohesion, production, military, commerce, logistics, and innovation
70. Replaced the old UUID icon set with the new crisp semantic icon package
71. Added climate generation inputs for randomized latitude, distance to sea, elevation cooling, and mountain rain shadow

## Ver0.1.1

Implemented features:

1. Windows graphical game window
2. Colored terrain rendering
3. Colored civilization territory overlay
4. Country border lines
5. Coast outline lines
6. Village icons
7. Year display
8. Side panel with civilization stats
9. Mouse tile inspection
10. Keyboard controls for simulation, auto-run, reset, and quit

## Ver0.1.0

Implemented features:

1. Procedural world generation
2. Terrain types: ocean, coast, grassland, forest, desert, mountain
3. Three default civilizations
4. Custom civilization creation
5. Editable civilization preferences
6. Population growth
7. Territory expansion
8. Border conflict and conquest
9. Random world events
10. Tile inspection and civilization status views

## Ver0.0.1

Goal:

Create the first text based civilization simulation prototype.

Planned features:

1. Create three civilizations
2. Simulate population growth
3. Simulate territory expansion
4. Simulate simple battles
5. Print daily results
