CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 -I. -Isrc
LDFLAGS ?= -lgdi32 -luser32 -lmsimg32 -lgdiplus -lcomdlg32 -mwindows
TARGET := world_sim.exe
SDL_TARGET ?= $(if $(filter Windows_NT,$(OS)),world_sim_sdl.exe,world_sim)
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 sdl3-ttf 2>/dev/null)
SDL_LDFLAGS ?= $(shell pkg-config --libs sdl3 sdl3-ttf 2>/dev/null)
APP_NAME ?= World Sim.app
PYTHON ?= $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null || printf python)
DEFAULT_TARGET ?= $(if $(filter Windows_NT,$(OS)),legacy,sdl)

SOURCES := \
	src/main.c \
	src/game/game.c \
	src/game/game_color_actions.c \
	src/game/game_country_actions.c \
	src/game/game_loop.c \
	src/game/game_setup_actions.c \
	src/game/game_worldgen.c \
	src/io/map_save.c \
	src/io/map_save_regions.c \
	src/core/event_log.c \
	src/core/event_log_history.c \
	src/core/game_state.c \
	src/core/country_focus.c \
	src/core/dirty_flags.c \
	src/core/profiler.c \
	src/core/render_snapshot.c \
	src/core/render_snapshot_keys.c \
	src/core/state_lock.c \
	src/core/worldgen_progress.c \
	src/data/country_names.c \
	src/data/province_names.c \
	src/data/game_tables.c \
	src/world/world_gen.c \
	src/world/world_seed.c \
	src/world/mountain_gen.c \
	src/world/terrain_query.c \
	src/world/world_smoothing.c \
	src/world/rivers.c \
	src/sim/simulation.c \
	src/sim/simulation_seed.c \
	src/sim/simulation_month.c \
	src/sim/simulation_scheduler.c \
	src/sim/simulation_worker.c \
	src/world/noise.c \
	src/world/ports.c \
	src/sim/ports.c \
	src/sim/maritime.c \
	src/sim/maritime_diag.c \
	src/sim/sea_nav.c \
	src/sim/route_potential.c \
	src/sim/sea_lanes.c \
	src/sim/territory_integrity.c \
	src/sim/vassal.c \
	src/sim/civilization_slots.c \
	src/sim/civilization_uid.c \
	src/sim/civ_colors.c \
	src/sim/disorder.c \
	src/sim/collapse.c \
	src/sim/population.c \
	src/sim/plague.c \
	src/sim/civilization_metrics.c \
	src/sim/decision_snapshot.c \
	src/sim/technology.c \
	src/sim/province.c \
	src/sim/province_partition.c \
	src/sim/region_boundary.c \
	src/sim/regions_config.c \
	src/sim/regions.c \
	src/sim/regions_validate.c \
	src/sim/regions_shape.c \
	src/sim/regions_spawn.c \
	src/sim/spawn.c \
	src/sim/expansion.c \
	src/sim/diplomacy.c \
	src/sim/diplomacy_borders.c \
	src/sim/diplomacy_contact.c \
	src/sim/diplomacy_names.c \
	src/sim/war.c \
	src/sim/war_front.c \
	src/sim/war_resolution.c \
	src/render/render.c \
	src/render/render_context.c \
	src/render/snapshot_ui.c \
	src/render/render_common.c \
	src/render/ui_format.c \
	src/render/cartography_layers.c \
	src/render/contour_paths.c \
	src/render/vector_paths.c \
	src/render/worldgen_progress_overlay.c \
	src/render/diplomacy_map_anim.c \
	src/render/map_render.c \
	src/render/map_highlight.c \
	src/render/terrain_present.c \
	src/render/region_render.c \
	src/render/snapshot_map_layers.c \
	src/render/map_labels.c \
	src/render/map_label_style.c \
	src/render/route_render.c \
	src/render/river_geometry.c \
	src/render/river_render.c \
	src/render/sea_lane_render.c \
	src/render/plague_render.c \
	src/render/plague_visual.c \
	src/render/pause_menu_render.c \
	src/render/panel_country.c \
	src/render/panel_country_actions.c \
	src/render/panel_country_cards.c \
	src/render/panel_country_events.c \
	src/render/panel_country_detail.c \
	src/render/panel_country_tech.c \
	src/render/panel_country_decision.c \
	src/render/panel_country_population.c \
	src/render/panel_country_resources.c \
	src/render/panel_country_diplomacy.c \
	src/render/panel_country_diplomacy_cards.c \
	src/render/panel_country_disorder.c \
	src/render/panel_country_diplomacy_hits.c \
	src/render/panel_population_page.c \
	src/render/panel_plague_page.c \
	src/render/panel_worldgen.c \
	src/render/panel_debug_worldgen.c \
	src/render/panel_debug_perf.c \
	src/render/panel_debug.c \
	src/render/panel_population.c \
	src/render/panel_info.c \
	src/render/panel_map.c \
	src/render/icons.c \
	src/ui/ui_theme.c \
	src/ui/ui_widgets.c \
	src/ui/color_picker.c \
	src/ui/pause_menu.c \
	src/ui/ui_actions.c \
	src/ui/ui_debug_input.c \
	src/ui/ui_invalidation.c \
	src/ui/ui_wheel.c \
	src/ui/ui_worldgen_layout.c \
	src/ui/ui_state.c \
	src/ui/ui_layout.c \
	src/ui/ui_map_input.c \
	src/ui/ui_sliders.c \
	src/ui/ui_forms.c \
	src/ui/ui_selection.c \
	src/ui/ui_snapshot_read.c \
	src/ui/ui.c

SDL_SOURCES := \
	src/main_sdl.c \
	src/game/game_color_actions.c \
	src/game/game_country_actions.c \
	src/game/game_loop.c \
	src/game/game_setup_actions.c \
	src/game/game_worldgen.c \
	src/platform/sdl_app.c \
	src/platform/sdl_country_input.c \
	src/platform/sdl_gdi_compat.c \
	src/platform/sdl_gdi_text.c \
	src/platform/sdl_input.c \
	src/platform/sdl_render.c \
	src/platform/sdl_render_ui.c \
	src/platform/sdl_text.c \
	src/platform/sdl_worldgen_form.c \
	src/platform/sdl_world.c \
	src/core/event_log.c \
	src/core/event_log_history.c \
	src/core/game_state.c \
	src/core/country_focus.c \
	src/core/dirty_flags.c \
	src/core/profiler.c \
	src/core/render_snapshot.c \
	src/core/render_snapshot_keys.c \
	src/core/state_lock.c \
	src/core/worldgen_progress.c \
	src/data/country_names.c \
	src/data/province_names.c \
	src/data/game_tables.c \
	src/world/world_gen.c \
	src/world/world_seed.c \
	src/world/mountain_gen.c \
	src/world/terrain_query.c \
	src/world/world_smoothing.c \
	src/world/rivers.c \
	src/sim/simulation.c \
	src/sim/simulation_seed.c \
	src/sim/simulation_month.c \
	src/sim/simulation_scheduler.c \
	src/sim/simulation_worker.c \
	src/world/noise.c \
	src/world/ports.c \
	src/sim/ports.c \
	src/sim/maritime.c \
	src/sim/maritime_diag.c \
	src/sim/sea_nav.c \
	src/sim/route_potential.c \
	src/sim/sea_lanes.c \
	src/sim/territory_integrity.c \
	src/sim/vassal.c \
	src/sim/civilization_slots.c \
	src/sim/civilization_uid.c \
	src/sim/civ_colors.c \
	src/sim/disorder.c \
	src/sim/collapse.c \
	src/sim/population.c \
	src/sim/plague.c \
	src/sim/civilization_metrics.c \
	src/sim/decision_snapshot.c \
	src/sim/technology.c \
	src/sim/province.c \
	src/sim/province_partition.c \
	src/sim/region_boundary.c \
	src/sim/regions_config.c \
	src/sim/regions.c \
	src/sim/regions_validate.c \
	src/sim/regions_shape.c \
	src/sim/regions_spawn.c \
	src/sim/spawn.c \
	src/sim/expansion.c \
	src/sim/diplomacy.c \
	src/sim/diplomacy_borders.c \
	src/sim/diplomacy_contact.c \
	src/sim/diplomacy_names.c \
	src/sim/war.c \
	src/sim/war_front.c \
	src/sim/war_resolution.c \
	src/render/diplomacy_map_anim.c \
	src/render/plague_visual.c \
	src/render/render_context.c \
	src/render/snapshot_ui.c \
	src/render/render_common.c \
	src/render/ui_format.c \
	src/render/contour_paths.c \
	src/render/river_geometry.c \
	src/render/river_render.c \
	src/render/icons.c \
	src/render/pause_menu_render.c \
	src/render/panel_info.c \
	src/render/panel_map.c \
	src/render/panel_country.c \
	src/render/panel_country_actions.c \
	src/render/panel_country_cards.c \
	src/render/panel_country_events.c \
	src/render/panel_country_detail.c \
	src/render/panel_country_tech.c \
	src/render/panel_country_decision.c \
	src/render/panel_country_population.c \
	src/render/panel_country_resources.c \
	src/render/panel_country_diplomacy.c \
	src/render/panel_country_diplomacy_cards.c \
	src/render/panel_country_disorder.c \
	src/render/panel_country_diplomacy_hits.c \
	src/render/panel_population_page.c \
	src/render/panel_plague_page.c \
	src/render/panel_worldgen.c \
	src/render/panel_debug_worldgen.c \
	src/render/panel_debug_perf.c \
	src/render/panel_debug.c \
	src/render/panel_population.c \
	src/ui/color_picker.c \
	src/ui/pause_menu.c \
	src/ui/ui_theme.c \
	src/ui/ui_widgets.c \
	src/ui/ui_layout.c \
	src/ui/ui_state.c \
	src/ui/ui_worldgen_layout.c

.PHONY: all legacy sdl sdl-windows mac-app clean check-text

all: $(DEFAULT_TARGET)

legacy: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

sdl: $(SDL_TARGET)

$(SDL_TARGET): $(SDL_SOURCES)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SDL_SOURCES) -o $(SDL_TARGET) $(SDL_LDFLAGS)

sdl-windows:
	$(MAKE) sdl SDL_TARGET=world_sim_sdl.exe

mac-app: sdl
	mkdir -p "$(APP_NAME)/Contents/MacOS" "$(APP_NAME)/Contents/Resources"
	cp "$(SDL_TARGET)" "$(APP_NAME)/Contents/MacOS/world_sim"
	cp -R data "$(APP_NAME)/Contents/Resources/"
	printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>' \
	'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
	'<plist version="1.0"><dict>' \
	'<key>CFBundleExecutable</key><string>world_sim</string>' \
	'<key>CFBundleIdentifier</key><string>local.world-sim</string>' \
	'<key>CFBundleName</key><string>World Sim</string>' \
	'<key>CFBundlePackageType</key><string>APPL</string>' \
	'<key>NSPrincipalClass</key><string>NSApplication</string>' \
	'<key>NSHighResolutionCapable</key><true/>' \
	'</dict></plist>' > "$(APP_NAME)/Contents/Info.plist"

clean:
	rm -f $(TARGET) world_sim world_sim_sdl.exe

check-text:
	$(PYTHON) tools/check_mojibake.py
