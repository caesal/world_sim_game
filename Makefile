CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 -I. -Isrc
LDFLAGS ?= -lgdi32 -luser32 -lmsimg32 -lgdiplus -mwindows
TARGET := world_sim.exe

SOURCES := \
	src/main.c \
	src/game/game.c \
	src/game/game_loop.c \
	src/core/game_state.c \
	src/core/dirty_flags.c \
	src/data/game_tables.c \
	src/world/world_gen.c \
	src/world/world_seed.c \
	src/world/terrain_query.c \
	src/world/world_smoothing.c \
	src/world/rivers.c \
	src/sim/simulation.c \
	src/sim/simulation_month.c \
	src/sim/simulation_scheduler.c \
	src/world/noise.c \
	src/world/ports.c \
	src/sim/ports.c \
	src/sim/maritime.c \
	src/sim/population.c \
	src/sim/plague.c \
	src/sim/civilization_metrics.c \
	src/sim/province.c \
	src/sim/province_partition.c \
	src/sim/region_boundary.c \
	src/sim/regions.c \
	src/sim/regions_shape.c \
	src/sim/regions_spawn.c \
	src/sim/spawn.c \
	src/sim/expansion.c \
	src/sim/diplomacy.c \
	src/sim/war.c \
	src/render/render.c \
	src/render/render_common.c \
	src/render/cartography_layers.c \
	src/render/border_paths.c \
	src/render/map_render.c \
	src/render/region_render.c \
	src/render/map_labels.c \
	src/render/maritime_render.c \
	src/render/plague_render.c \
	src/render/plague_visual.c \
	src/render/panel_selection.c \
	src/render/panel_country.c \
	src/render/panel_population_page.c \
	src/render/panel_plague_page.c \
	src/render/panel_worldgen.c \
	src/render/panel_debug.c \
	src/render/panel_population.c \
	src/render/panel_info.c \
	src/render/panel_diplomacy.c \
	src/render/panel_map.c \
	src/render/icons.c \
	src/ui/ui_theme.c \
	src/ui/ui_widgets.c \
	src/ui/ui_worldgen_layout.c \
	src/ui/ui_state.c \
	src/ui/ui_layout.c \
	src/ui/ui_sliders.c \
	src/ui/ui_forms.c \
	src/ui/ui.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
