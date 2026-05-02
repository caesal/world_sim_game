# Version Log

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
