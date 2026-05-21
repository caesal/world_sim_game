#include "platform/sdl_world.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "game/game_worldgen.h"
#include "sim/simulation_month.h"
#include "ui/ui_types.h"
#include "world/world_gen.h"

void sdl_world_reset_blank(void) {
    game_clear_world_tiles();
}

void sdl_world_generate_default(int civs_requested, int map_size) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;

    ocean_slider = config.ocean;
    continent_slider = config.continent;
    relief_slider = config.relief;
    moisture_slider = config.moisture;
    drought_slider = config.drought;
    vegetation_slider = config.vegetation;
    bias_forest_slider = config.bias_forest;
    bias_desert_slider = config.bias_desert;
    bias_mountain_slider = config.bias_mountain;
    bias_wetland_slider = config.bias_wetland;
    pending_map_size = map_size;
    initial_civ_count = clamp(civs_requested, 0, MAX_CIVS);
    game_request_new_world_with_callbacks(NULL, NULL);
}

int sdl_world_step_months(int months) {
    int i;
    int completed = 0;

    if (!world_generated || months <= 0) return 0;
    for (i = 0; i < months; i++) {
        state_write_lock();
        simulation_month_run_blocking();
        state_write_unlock();
        completed++;
    }
    if (completed > 0) render_snapshot_publish_from_live_state();
    return completed;
}
