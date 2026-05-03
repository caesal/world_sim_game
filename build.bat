@echo off
setlocal

gcc -O2 -Wall -Wextra -I. -Isrc ^
  src\main.c ^
  src\game\game.c ^
  src\core\game_state.c ^
  src\data\game_tables.c ^
  src\world\world_gen.c ^
  src\world\terrain_query.c ^
  src\world\rivers.c ^
  src\sim\simulation.c ^
  src\world\noise.c ^
  src\world\ports.c ^
  src\sim\civilization_metrics.c ^
  src\sim\province.c ^
  src\sim\expansion.c ^
  src\sim\diplomacy.c ^
  src\sim\war.c ^
  src\render\render.c ^
  src\render\render_common.c ^
  src\render\map_render.c ^
  src\render\panel_info.c ^
  src\render\panel_diplomacy.c ^
  src\render\panel_map.c ^
  src\render\icons.c ^
  src\ui\ui_layout.c ^
  src\ui\ui.c ^
  -o world_sim.exe -lgdi32 -luser32 -lmsimg32 -lgdiplus -mwindows
if errorlevel 1 exit /b %errorlevel%

endlocal
