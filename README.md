# World Sim Game

This is my first civilization sandbox simulation game.

Goal:
Create a small world map with several civilizations that can expand, form borders, and survive automatically.

## Current Prototype

Ver0.3.6.b is a Windows graphical sandbox prototype written in C.

Ver0.3.6.b replaces the former single-column world setup form with a
presentation-only four-tab control surface: Physical Terrain, Climate &
Vegetation, Hydrology & Regions, and Legacy Modules. The new visual controls
and the complete legacy form are bidirectionally synchronized through the
existing integer configuration fields, so this release does not change world
generation algorithms, phase order, save compatibility, or simulation rules.
It adds a seven-axis read-only world fingerprint, direct and crossed relief
handles, a four-corner climate tendency envelope, river and natural-region
visual selectors, and a second synchronized Initial Civilizations entry.

The climate background is explicitly an explanatory tendency map, not a
prediction of generated biome coverage. Statistical calibration of that
background is a separate later task and is not part of this release.

Ver0.3.6.a rebuilt the static physical world pipeline around coherent
elevation, broad mountain systems, a fixed annual wind field, orographic
moisture transport, climate-aware terrain, connected drainage, lakes, river
confluences, mouths, deltas, and hydroclimate. Physical geography is generated
once, published atomically, persisted as save-version-20 state, and remains
immutable while the later civilization simulation runs.

The release removes threshold-generated diagonal coast meshes at their source,
supports low-ocean Extreme worlds with exact-sized river-path ownership, and
prebuilds immutable map assets so panning, zooming, and Geography/Climate
switching do not rerasterize static terrain, coasts, water, rivers, or wind.
Rivers progressively reveal complete downstream-connected systems at
100/150/225/300 percent zoom, with the full generated network at 300 percent.

The named global plague model from Ver0.3.6 remains intact: bounded spore
budgets, batched route-aware spread, fixed episode severity, exact
monthly-equivalent mortality, duration-based immunity, plague disorder,
bilingual structured announcements, persistent history, and linked outbreak
probability controls all remain supported.

The Plague panel now keeps fog and linked outbreak-probability controls above
Live, Impact, and History views. Live shows the active episode or most recently
completed episode. Impact prioritizes affected countries and the five cities
with the most plague deaths. History compares the latest seven completed
episodes across type, severity, duration, deaths, cities, countries, and spores.
Probability changes apply immediately while no episode is active; changes made
during an active episode are persisted as pending and apply when it ends.

Future performance, stutter, scheduler, rendering, map-display, simulation
speed, or Phase 6 validation must use an Extreme map, at least 26 placed
civilizations, randomized physical map parameters, randomized advanced terrain
preferences, more than 600 natural regions, 5x/max speed until at least five
distinct civilizations reach technology stage 5, verify deep-sea routes
transition from hidden/unrevealed to visible/revealed after unlock, include
maximized Debug / Performance evidence, and use non-disruptive window handling
when another fullscreen application is active.

Validation note: Ver0.3.6.b includes deterministic adapter, interaction,
layout, asset-cache, bilingual artifact, legacy-preservation, and
two-way-synchronization coverage. Release acceptance also requires both build
paths, static/text gates, generated-world GUI validation, performance/resource
checks, and a fresh final-source AGENTS Rule39 run after the source scope is
frozen.
Future performance, UI, map-display, simulation-speed, diplomacy, war, vassal,
collapse, enclave, route, marker, plague, or population balance changes must
remain evidence-based and pass the strict validation gate for the specific scope
involved.

You can:

1. Generate a random 1920x1080 visual world map from continuous elevation, moisture, and temperature fields
2. Watch civilizations expand as colored territory on a higher-detail visual map
3. See country borders, coast outlines, cities, and year/month progress
4. Simulate month by month with five auto-run speed targets
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
20. Use selectable Small, Medium, Large, and Extreme internal map grids with crisp tile rendering
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
44. Create named alliances, show them in a dedicated Alliance map view, and inspect alliance-aware diplomacy state
45. Use player country actions for war, peace, vassalization, alliance formation, and alliance withdrawal
46. Inspect relation score factors through diplomacy tooltips and Debug / Performance render spike attribution
47. Keep Extreme-map 5x presentation smoother across Alliance, Country, Province, Routes, Geography, and Climate views
44. Use city stage icons for outpost, village, town, city, capital, and harbor markers
45. Show Chinese two-character stat labels beside icons when the UI language is Chinese
46. Keep versioned design and review documents under `docs/official` and `docs/unofficial` with version-matched filenames
47. Keep `.c` and `.h` files under the 500-line module size rule
48. Use unified clay presentation primitives for World Setup controls without changing world-generation rules
48. Draw smoother cartographic country borders, province borders, coastlines, political fills, labels, and subtle map grid overlays
49. Render continuous river path objects instead of scattered tile-center fragments
50. Restyle city, capital, harbor, hill, and mountain map markers toward an old political map look while reusing existing assets
51. Rebalance diplomacy so prosperous neighboring civilizations can stabilize relations through trade fit instead of drifting into tension only because they are self-sufficient
52. Add explicit maritime route paths for port-to-port contact, migration, diplomacy exposure, overseas expansion, and dashed sea-lane rendering
53. Add city-level age/sex population cohorts, derived country population summaries, soldier casualty population loss, and an Info-tab population pyramid
54. Replace flat plague events with persistent city outbreaks, percentage deaths, spread pressure, disorder impact, and dark green map visualization
55. Cache expensive map render layers across ordinary repaints and avoid duplicate maritime route rebuilds during monthly simulation
56. Cache diplomacy border contacts and population summaries so monthly updates do less repeated full-map aggregation
57. Cache country decision diagnostics on the simulation side so RenderSnapshot publishing no longer recomputes them under the state read lock
57. Split core shared types into narrower `constants.h`, `world_types.h`, and `sim_types.h` headers while keeping `game_types.h` as the compatibility entry point
58. Document the current map-rendering and river-polish diagnostic for the next targeted cleanup pass
59. Use a fast transformed cached-map preview while wheel zooming or right-drag panning, then rebuild high-quality layers after input settles
60. Cache the 800x600 base terrain bitmap separately from zoom and pan layout
61. Draw labels outside the expensive cached map layer so name edits repaint correctly without forcing full map rebuilds
62. Reuse the full-window paint backbuffer instead of allocating one on every paint
63. Rebuild maritime routes only when port/city route data is marked dirty
64. Rebuild diplomacy contact scans only when territory/contact data changes
65. Skip the full-map plague region overlay when no city has an active plague
66. Drive rendering from a fixed frame timer while simulation speed controls schedule month ticks through elapsed time
67. Use a simulation scheduler wrapper so auto-run queues at most one pending month of work per frame
68. Add central dirty flags for world, territory, province, population, plague, maritime, and label render state
69. Split population-only cache invalidation from territory/province invalidation so ordinary births, deaths, and migration do not force border/coast cache rebuilds
70. Draw city markers and plague city cores as lightweight dynamic overlays outside the expensive cached map layer
71. Start on a blank ungenerated map instead of auto-building a world at launch
72. Choose Small 640x360, Medium 800x450, or Large 960x540 active map sizes from the Map tab
73. Default map generation sliders to 50 and default initial civilizations to 0
74. Use the selected active map dimensions for generation, rendering layout, tile selection, rivers, ports, simulation scans, and map caches
75. Run monthly simulation through smaller scheduler phases instead of one large frame-blocking step
76. Use speed-aware scheduler budgets so faster speeds can process more bounded work per frame
77. Cache terrain, political, coast, and border/static map layers separately so dynamic overlays do not rebuild the whole map
78. Draw maritime routes and plague overlays as dynamic layers above the static cached map
79. Animate plague visuals with render-only interpolation, soft dark-green infection clouds, pulsing city cores, and fading infected sea-route intensity
80. Keep plague rendering read-only from simulation state while still reflecting monthly plague severity
81. Skip territory recalculation in the monthly phase when expansion did not change ownership or city count
82. Fix Chinese age-structure labels in the population pyramid
83. Generate continuous mountain chains with ridges, branches, and foothills before river and natural-region passes
84. Strengthen natural-region boundary costs around mountains, canyons, rivers, coastlines, and climate/ecology changes
85. Track technology stages 1-10 for every civilization, with innovation and resources controlling progress
86. Apply technology effects to expansion pace, resource output, deep-sea stability, defense, battle odds, and long-held vassals
87. Use reusable Claymorphism widgets for selected top-bar, bottom-bar, side-panel handle, and pause-menu controls
88. Preserve the application icon in both Makefile and build.bat builds through the shared Windows resource object
89. Launch with `--no-activate` for validation runs that must not steal focus from the user's foreground app
90. Draw the collapsed side-panel handle without an opaque square cache artifact over the map
87. Use 0-100 disorder with monthly recovery/pressure drift, war-death and plague-death disorder impacts, and decade collapse checks
88. Rework wars into 2-year battles using current soldiers, technology modifiers, real population casualties, peace pressure, and 20% bordering-province cession
89. Keep normal expansion focused on neighboring unowned natural regions before considering overseas targets
90. Add a Country Dashboard back-to-list control so selected-country detail no longer traps the user away from all countries
91. Smooth maritime route rendering so sea lanes curve through existing route points instead of exposing every pathfinding bend
92. Refresh political fills, country borders, province borders, and labels immediately after a natural region is claimed
93. Show resource, population, and technology modifiers as final effective percentages with their technology/disorder components visible
94. Keep the performance/debug panel available to verify scheduler backlog, slow phases, route diagnostics, and render cache rebuilds
95. Treat vassals as direct subordinate countries instead of independent diplomatic actors
96. Route declarations against vassals to their overlord and prevent vassals from starting ordinary wars
97. Let overlords call up to 70% of each direct vassal army while tracking vassal casualties in the unified army ledger
98. Transfer 40% of vassal non-money resource output to the overlord by deducting it from the vassal summary
99. Add vassal governance burden to disorder as `min(100, 10n)` for direct vassal count
100. Release vassals on overlord collapse and keep successor states independent after a vassal collapse
101. Use a route-potential graph to precompute potential port nodes and shallow/deep route edges after world generation
102. Keep side-panel tab clicks responsive at max speed by repainting the side panel through a narrow partial path instead of a synchronous full-window redraw
103. Defer heavy full-map repaint work while input is waiting and reuse the cached backbuffer so 5x interaction remains responsive under render/simulation pressure
102. Activate occupied region port sites deterministically so ordinary map sea lanes come from the same graph shown in the route-potential debug layer
103. Render shallow and deep water as the visible water categories, with a soft visual gradient while gameplay still uses hard shallow/deep thresholds
104. Collapse or expand the right sidebar while centering the map inside the actual available viewport
105. Use custom dark Random buttons and clearer World-tab form layout for civilization and world-generation setup
106. Show world-generation progress as separate overall and current-stage progress while hiding half-built maps
107. Assign stable bilingual province names from the province-name table and redraw labels when the UI language changes
108. Default ordinary map display to the political layer and keep composite all-layer display out of the player-facing selector
109. Draw sea-lane dashes with stable world-distance rhythm and clearer shallow/deep colors and widths
110. Gate diplomacy peace, tension, war starts, and map animations on current land or active sea-lane contact
111. Fade disconnected known peace/tension relationships instead of allowing them to escalate without a current front
112. Copy cached city, diplomacy, lane, and plague presentation data into RenderSnapshot instead of recomputing those summaries while holding the snapshot read lock
113. Prime snapshot presentation caches before forced worldgen, load, and manual-action publishes so immediate UI views do not start with empty cache data
114. Treat dead city cache slots as valid zero-summary entries so they do not keep city snapshot sections dirty forever
115. Treat every natural region as one province with exactly one stable generated city slot
116. Make port cities a subtype of that one city slot, with island landmasses guaranteed at least one port city
117. Transfer war cessions by natural region rather than by legacy city-index province ids
118. Show neutral generated settlement slots on the Regions map layer while hiding them on normal gameplay layers
119. Show direct vassals in compact overview rows with colored name cells plus Release and Annex actions
120. Select and locate a direct vassal by clicking its colored name cell without pausing the simulation
121. Keep route-potential legend entries route-only while political mode shows city, capital, harbor, and harbor-capital glyph names
122. Resolve active wars by attacker/defender peace-pressure roles instead of letting higher disorder invert the winner
123. Record visible post-war history labels for surrender, military victory/defeat, negotiated truce, offensive halt, and severed-front interruption
124. Vassalize a defeated country when no regions are actually ceded or when post-settlement disorder and cohesion cross the severe-instability thresholds
125. Deduct cohesion when any civilization loses its capital, and give collapse/enclave successor countries parent cohesion plus a bounded random bonus
126. Apply Claymorphism Phase 4 presentation to Country lists, selected summaries, overview metric chips, action pills, diplomacy tabs, diplomacy cards, semantic relation accents, truce spacing, and vassal hierarchy rows
127. Sort War & Truce relations with active wars first, then truces by remaining duration from longest to shortest
128. Use selected-country action buttons for Declare War, Peace, Vassalize, and Civil Unrest in one row
129. Target Declare War and Vassalize commands with dynamic red or purple arrows and top stacked notifications
130. End a selected country's active direct wars through no-winner Peace commands
131. Route side-panel tab switching through normal invalidation to reduce direct-paint flicker
132. Run one bilingual named global plague episode at a time on a 20-year schedule with a rolling 100-year outbreak cap
133. Spread plague through cached land, shallow-sea, and unlocked deep-sea contacts with bounded batched spore decisions
134. Track fixed episode severity, exact monthly-equivalent mortality, duration-based immunity, plague disorder, and seven completed-episode comparisons
135. Inspect active or latest plague data through Live, affected countries and top-five cities through Impact, and completed episodes through History
136. Adjust linked No plague, Small, Medium, and Large outbreak probabilities while preserving an exact total of 100 percent
137. Save effective and pending plague probabilities in save version 19, applying active-episode changes only after that episode ends

## Controls

1. `Space` starts or pauses auto-run
2. The bottom play button also starts or pauses auto-run
3. The first speed button sets observation speed, 10 seconds per month
4. The second speed button sets slow speed, 5 seconds per month
5. The third speed button sets normal speed, 1 second per month
6. The fourth speed button sets fast speed, 0.25 seconds per month
7. The fifth speed button sets maximum target speed, 0.1 seconds per month
8. `F1` adds a civilization from the right-side form
9. `F2` applies the form to the selected civilization
10. `F5` generates a new random world using the right-side world setup
11. `R` also generates a new random world when the map has keyboard focus
12. `Esc` opens the pause menu with Resume Game, Version Log, Save Map, Load Map, and Exit Game
13. Left mouse click selects a tile or civilization
14. Use the right-side tabs to switch between info, civilization controls, diplomacy, and map generation
15. Drag the panel divider to resize the side controls
16. Mouse wheel zooms the map around the cursor
17. In the Map tab, click mode buttons to switch map layers
18. In the Map tab, drag generation sliders to adjust the next generated world
19. Hold right mouse button and drag the map to pan
20. Hover over compact stat blocks in the right panel to see their meaning
21. In the Plague panel, adjust the four linked outbreak probabilities and use Apply or Reset; active-episode changes become effective after the episode ends

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
16. `docs/official` contains current universal documentation and versioned change summaries
17. `docs/unofficial` contains historical side docs, review notes, design PDFs, and the version log used during development
