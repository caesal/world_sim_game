#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/profiler.h"
#include "core/state_lock.h"
#include "render/plague_visual.h"
#include "sim/simulation_scheduler.h"
#include "sim/simulation_worker.h"

static DWORD last_frame_tick = 0;

void game_loop_reset(void) {
    last_frame_tick = GetTickCount();
    simulation_worker_start();
    simulation_worker_reset_scheduler();
    profiler_reset();
}

int game_loop_tick_frame(void) {
    DWORD now = GetTickCount();
    int elapsed;
    int did_visual = 0;
    int completed_months = 0;

    simulation_worker_start();
    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    state_read_lock();
    did_visual = plague_visual_tick(elapsed);
    state_read_unlock();
    profiler_record_frame(elapsed, simulation_worker_last_budget_ms(),
                          simulation_worker_last_used_ms(),
                          simulation_worker_actual_ms_per_month(),
                          game_loop_pending_months(), game_loop_simulation_overloaded());
    completed_months = simulation_worker_take_visual_tick();
    return did_visual || completed_months > 0 || map_interaction_preview || dirty_any_render();
}

int game_loop_actual_ms_per_month(void) {
    return simulation_worker_actual_ms_per_month();
}

int game_loop_pending_months(void) {
    return simulation_worker_pending_months();
}

int game_loop_simulation_overloaded(void) {
    return simulation_worker_overloaded();
}

int game_loop_snapshot_age_ms(void) {
    return simulation_worker_snapshot_age_ms();
}

const char *game_loop_worker_status(void) {
    return simulation_worker_status();
}
