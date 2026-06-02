#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/plague_perf.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "render/diplomacy_map_anim.h"
#include "render/plague_visual.h"
#include "sim/simulation_scheduler.h"
#include "sim/simulation_worker.h"

#include <stdio.h>
#include <string.h>

static DWORD last_frame_tick = 0;
static int last_redraw_flags;
static int last_completed_months;
static int last_completed_month_map_redraw;
static int pending_presentation_redraw;
static int render_presentation_throttled;
static DWORD last_presentation_redraw_tick;
static char last_map_redraw_reason[160] = "none";

#define PRESENTATION_MAP_REDRAW_MASK \
    (GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_MAP_STATIC | GAME_REDRAW_PLAGUE_OVERLAY)

static void append_map_reason(char *buffer, int buffer_size, const char *reason) {
    int used;
    if (!buffer || buffer_size <= 0 || !reason || !reason[0]) return;
    used = (int)strlen(buffer);
    if (used > 0) used += snprintf(buffer + used, buffer_size - used, "+");
    if (used < buffer_size) snprintf(buffer + used, buffer_size - used, "%s", reason);
}

void game_loop_reset(void) {
    last_frame_tick = GetTickCount();
    pending_presentation_redraw = GAME_REDRAW_NONE;
    render_presentation_throttled = 0;
    last_presentation_redraw_tick = 0;
    simulation_worker_start();
    simulation_worker_reset_scheduler();
    profiler_reset();
}

static int presentation_throttle_interval_ms(int redraw) {
    RuntimeProfilerSnapshot perf;
    int actual_ms;
    int target_ms;
    int overloaded;
    int worker_throttled;
    if (!auto_run || speed_index < SPEED_COUNT - 1) return 0;
    if (redraw & GAME_REDRAW_FULL) return 0;
    if (map_interaction_preview) return 0;
    profiler_snapshot(&perf);
    actual_ms = simulation_worker_actual_ms_per_month();
    target_ms = SPEED_MS[clamp(speed_index, 0, SPEED_COUNT - 1)];
    overloaded = simulation_worker_overloaded();
    worker_throttled = simulation_worker_presentation_throttled();
    if (!worker_throttled && !overloaded &&
        perf.render_avg_ms <= 34 && perf.render_peak_ms <= 80) {
        return 0;
    }
    if (overloaded || actual_ms > target_ms * 12 || perf.render_avg_ms > 180) return 3000;
    if (actual_ms > target_ms * 6 || perf.render_avg_ms > 40) return 1500;
    return 250;
}

static int max_speed_presentation_overloaded(void) {
    return auto_run && world_generated && speed_index >= SPEED_COUNT - 1 &&
           (simulation_worker_presentation_throttled() || simulation_worker_overloaded());
}

static int coalesce_presentation_redraw(int redraw, DWORD now) {
    int interval = presentation_throttle_interval_ms(redraw);
    int combined = redraw | pending_presentation_redraw;
    int immediate;
    int map_redraw;
    if (!combined) return GAME_REDRAW_NONE;
    if (combined & GAME_REDRAW_FULL) {
        pending_presentation_redraw = GAME_REDRAW_NONE;
        render_presentation_throttled = 0;
        last_presentation_redraw_tick = now;
        return combined;
    }
    if (interval <= 0) {
        pending_presentation_redraw = GAME_REDRAW_NONE;
        render_presentation_throttled = 0;
        last_presentation_redraw_tick = now;
        return combined;
    }
    immediate = combined & ~PRESENTATION_MAP_REDRAW_MASK;
    map_redraw = combined & PRESENTATION_MAP_REDRAW_MASK;
    if (!map_redraw) {
        pending_presentation_redraw = GAME_REDRAW_NONE;
        render_presentation_throttled = 0;
        return immediate;
    }
    if (interval > 0 && last_presentation_redraw_tick > 0 &&
        (int)(now - last_presentation_redraw_tick) < interval) {
        pending_presentation_redraw = map_redraw;
        render_presentation_throttled = 1;
        return immediate;
    }
    pending_presentation_redraw = GAME_REDRAW_NONE;
    render_presentation_throttled = 0;
    last_presentation_redraw_tick = now;
    return immediate | map_redraw;
}

int game_loop_tick_frame(void) {
    DWORD now = GetTickCount();
    int elapsed;
    int did_visual = 0;
    int completed_months = 0;
    int redraw = GAME_REDRAW_NONE;
    char map_reason[160] = "";

    simulation_worker_start();
    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    plague_perf_begin_frame();
    did_visual = plague_visual_tick(elapsed);
    profiler_record_frame(elapsed, simulation_worker_last_budget_ms(),
                          simulation_worker_last_used_ms(),
                          simulation_worker_actual_ms_per_month(),
                          game_loop_pending_months(), game_loop_simulation_overloaded());
    completed_months = simulation_worker_take_visual_tick();
    if (completed_months > 0) redraw |= GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                        GAME_REDRAW_SIDE_PANEL;
    if (did_visual && !max_speed_presentation_overloaded()) {
        redraw |= GAME_REDRAW_PLAGUE_OVERLAY;
        append_map_reason(map_reason, sizeof(map_reason), "plague-animation");
    } else if (did_visual) {
        plague_perf_note_invalidation_suppressed(1);
    }
    if (diplomacy_map_anim_active()) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        append_map_reason(map_reason, sizeof(map_reason), "diplomacy-animation");
    }
    if (map_interaction_preview) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        append_map_reason(map_reason, sizeof(map_reason), "map-interaction-preview");
    }
    if (dirty_render_terrain() || dirty_render_political() || dirty_render_coast() ||
        dirty_render_hydrology() || dirty_render_borders()) {
        redraw |= GAME_REDRAW_MAP_STATIC;
        append_map_reason(map_reason, sizeof(map_reason), "static-dirty");
    }
    if (dirty_render_plague()) {
        if (!plague_perf_visuals_allowed() || max_speed_presentation_overloaded()) {
            plague_perf_note_invalidation_suppressed(1);
            dirty_clear_render_plague();
        } else {
            redraw |= GAME_REDRAW_PLAGUE_OVERLAY;
            append_map_reason(map_reason, sizeof(map_reason), "plague-dirty");
        }
    }
    if (dirty_render_maritime() || dirty_render_labels()) {
        redraw |= GAME_REDRAW_MAP_DYNAMIC;
        if (dirty_render_maritime()) append_map_reason(map_reason, sizeof(map_reason), "maritime");
        if (dirty_render_labels()) append_map_reason(map_reason, sizeof(map_reason), "labels");
    }
    redraw = coalesce_presentation_redraw(redraw, now);
    last_redraw_flags = redraw;
    last_completed_months = completed_months;
    last_completed_month_map_redraw = completed_months > 0 &&
        (redraw & (GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_MAP_STATIC | GAME_REDRAW_PLAGUE_OVERLAY));
    snprintf(last_map_redraw_reason, sizeof(last_map_redraw_reason), "%s", map_reason[0] ? map_reason : "none");
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
    return simulation_worker_presentation_throttled() || render_presentation_throttled;
}

const char *game_loop_worker_status(void) {
    return simulation_worker_status();
}

int game_loop_last_redraw_flags(void) { return last_redraw_flags; }
int game_loop_last_completed_months(void) { return last_completed_months; }
int game_loop_last_completed_month_map_redraw(void) { return last_completed_month_map_redraw; }
const char *game_loop_last_map_redraw_reason(void) { return last_map_redraw_reason; }
