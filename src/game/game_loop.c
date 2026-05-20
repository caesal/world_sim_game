#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "render/diplomacy_map_anim.h"
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
    int redraw = GAME_REDRAW_NONE;

    simulation_worker_start();
    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    did_visual = plague_visual_tick(elapsed);
    profiler_record_frame(elapsed, simulation_worker_last_budget_ms(),
                          simulation_worker_last_used_ms(),
                          simulation_worker_actual_ms_per_month(),
                          game_loop_pending_months(), game_loop_simulation_overloaded());
    completed_months = simulation_worker_take_visual_tick();
    if (completed_months > 0) redraw |= GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                        GAME_REDRAW_MAP_DYNAMIC;
    if (did_visual || diplomacy_map_anim_active()) redraw |= GAME_REDRAW_MAP_DYNAMIC;
    if (map_interaction_preview) redraw |= GAME_REDRAW_MAP_DYNAMIC;
    if (dirty_render_terrain() || dirty_render_political() || dirty_render_coast() ||
        dirty_render_hydrology() || dirty_render_borders()) {
        redraw |= GAME_REDRAW_MAP_STATIC;
    }
    if (dirty_render_maritime() || dirty_render_plague() || dirty_render_labels()) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
    }
    return redraw;
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
    return render_snapshot_age_ms();
}

const char *game_loop_worker_status(void) {
    return simulation_worker_status();
}
