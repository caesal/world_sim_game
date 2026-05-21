#include "platform/sdl_app.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "platform/sdl_world.h"
#include "sim/simulation_month.h"
#include "sim/simulation_worker.h"

#include <stdio.h>
#include <string.h>

static int run_sdl_smoke(void) {
    int result;
    sdl_world_generate_default(25, MAP_SIZE_SMALL);
    sdl_world_step_months(1);
    printf("world_generated=%d map=%dx%d civs=%d cities=%d year=%d month=%d\n",
           world_generated, map_w, map_h, civ_count, city_count, year, month);
    result = world_generated && civ_count > 0 && city_count > 0 ? 0 : 1;
    simulation_worker_shutdown();
    render_snapshot_shutdown();
    return result;
}

static int run_sdl_tech10_smoke(void) {
    int month_index;
    int max_stage = 0;
    int max_civ = -1;

    sdl_world_generate_default(25, MAP_SIZE_SMALL);
    for (month_index = 0; month_index < 1600 * 12; month_index++) {
        int i;
        state_write_lock();
        simulation_month_run_blocking();
        state_write_unlock();
        for (i = 0; i < civ_count; i++) {
            if (!civs[i].alive) continue;
            if (civs[i].tech_stage > max_stage) {
                max_stage = civs[i].tech_stage;
                max_civ = i;
            }
            if (civs[i].tech_stage >= 10) {
                printf("reached_stage10=yes elapsed_months=%d civ=%d stage=%d year=%d month=%d\n",
                       month_index + 1, i, civs[i].tech_stage, year, month);
                simulation_worker_shutdown();
                render_snapshot_shutdown();
                return 0;
            }
        }
    }
    printf("reached_stage10=no elapsed_months=%d max_stage=%d civ=%d year=%d month=%d\n",
           month_index, max_stage, max_civ, year, month);
    simulation_worker_shutdown();
    render_snapshot_shutdown();
    return 2;
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1] && strcmp(argv[1], "--smoke") == 0) return run_sdl_smoke();
    if (argc > 1 && argv[1] && strcmp(argv[1], "--smoke-tech10") == 0) return run_sdl_tech10_smoke();
    if (argc > 1 && argv[1] && strcmp(argv[1], "--render-smoke") == 0) {
        return run_sdl_render_smoke(argc > 2 ? argv[2] : NULL);
    }
    return run_sdl_game();
}
