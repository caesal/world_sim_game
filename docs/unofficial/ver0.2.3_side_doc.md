# Ver0.2.3 Side Doc

## Scope

Ver0.2.3 focuses on pause/save usability and keeping the current UI/simulation fixes documented.

## Changes

- Added a first pause-menu action: **Resume Game / 返回游戏**.
- Save Game now opens a standard save dialog, so the player can choose the folder and rename the `.wsgmap` file before writing.
- Updated the visible version string to `0.2.3`.
- Updated README and version log notes for the pause/save flow and current system changes.

## Validation Plan

- Build with `.\build.bat`.
- Run a deterministic executable smoke/probe with `world_sim.exe --probe-expansion`.
- Confirm touched `.c/.h` files stay under 500 lines.
- Confirm stage 10 technology UI/logic is still reachable through existing technology helpers and country technology panel code.

## Validation Results

- `.\build.bat`: passed.
- `world_sim.exe --probe-expansion`: completed through year 500.
- Probe sanity reported new civilizations start at stage 0, stage 0 has no technology bonuses, stage 1 expansion is 110%, and stage 2 expansion is 120%.
- Expansion probe reached 92% active ownership by year 50 and 92% at year 500 on the deterministic medium-map run.
- Touched `.c/.h` files checked in this pass are below 500 lines.
- Stage 10 display/logic paths were checked statically through `technology_stage_progress_percent`, `technology_deep_sea_plague_percent`, and the country technology panel completion branch.

## Notes

- The save dialog is a UI-side file picker only; it does not change the save format.
- The resume button only closes the pause overlay and does not mutate simulation state.
- GUI-only checks such as clicking through every tab to stage 10 still require a manual visual pass in the Windows app.
