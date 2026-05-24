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
static int last_redraw_flags;
static int last_completed_months;
static int last_completed_month_map_redraw;
static const char *last_map_redraw_reason = "none";

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
    int completed_month_requested_map = 0;
    const char *map_reason = "none";

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
                                        GAME_REDRAW_SIDE_PANEL;
    if (did_visual) {
        redraw |= GAME_REDRAW_PLAGUE_OVERLAY;
        map_reason = "plague-animation";
    }
    if (diplomacy_map_anim_active()) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        if (map_reason[0] == 'n') map_reason = "diplomacy-animation";
    }
    if (map_interaction_preview) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        if (map_reason[0] == 'n') map_reason = "map-interaction-preview";
    }
    if (dirty_render_terrain() || dirty_render_political() || dirty_render_coast() ||
        dirty_render_hydrology() || dirty_render_borders()) {
        redraw |= GAME_REDRAW_MAP_STATIC;
        if (map_reason[0] == 'n') map_reason = "static-dirty";
    }
    if (dirty_render_plague()) {
        redraw |= GAME_REDRAW_PLAGUE_OVERLAY;
        if (map_reason[0] == 'n') map_reason = "plague-dirty";
    }
    if (dirty_render_maritime() || dirty_render_labels()) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        if (map_reason[0] == 'n') {
            map_reason = dirty_render_maritime() && dirty_render_labels() ? "maritime+labels" :
                         dirty_render_maritime() ? "maritime" : "labels";
        }
    }
    last_redraw_flags = redraw;
    last_completed_months = completed_months;
    last_completed_month_map_redraw = completed_months > 0 && completed_month_requested_map;
    last_map_redraw_reason = map_reason;
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

int game_loop_presentation_backlog(void) {
    return simulation_worker_visual_backlog();
}

int game_loop_visual_coalesced_months(void) {
    return simulation_worker_visual_coalesced_months();
}

int game_loop_presentation_throttled(void) {
    return simulation_worker_presentation_throttled();
}

const char *game_loop_worker_status(void) {
    return simulation_worker_status();
}

int game_loop_last_redraw_flags(void) { return last_redraw_flags; }
int game_loop_last_completed_months(void) { return last_completed_months; }
int game_loop_last_completed_month_map_redraw(void) { return last_completed_month_map_redraw; }
const char *game_loop_last_map_redraw_reason(void) { return last_map_redraw_reason; }
