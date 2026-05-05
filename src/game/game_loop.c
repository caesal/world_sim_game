#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/plague_visual.h"
#include "sim/simulation_scheduler.h"

static DWORD last_frame_tick = 0;
static int sim_accumulator_ms = 0;

void game_loop_reset(void) {
    last_frame_tick = GetTickCount();
    sim_accumulator_ms = 0;
    sim_scheduler_reset();
}

int game_loop_tick_frame(void) {
    DWORD now = GetTickCount();
    int elapsed;
    int did_work = 0;
    int did_visual = 0;
    int budget = 1;

    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    if (auto_run) {
        int interval = SPEED_MS[clamp(speed_index, 0, 2)];
        sim_accumulator_ms += elapsed;
        if (sim_accumulator_ms >= interval) {
            sim_accumulator_ms -= interval;
            if (sim_accumulator_ms > interval) sim_accumulator_ms = 0;
            sim_scheduler_request_month();
        }
    } else {
        sim_accumulator_ms = 0;
    }

    if (sim_scheduler_has_pending_work()) {
        if (auto_run && speed_index == 1) budget = 2;
        else if (auto_run && speed_index >= 2) budget = 4;
        did_work = sim_scheduler_run_budget(budget);
    }
    did_visual = plague_visual_tick(elapsed);
    return did_work || did_visual || map_interaction_preview || dirty_any_render();
}
