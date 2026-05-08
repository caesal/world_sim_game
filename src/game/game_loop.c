#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/profiler.h"
#include "render/plague_visual.h"
#include "sim/simulation_scheduler.h"

static DWORD last_frame_tick = 0;
static DWORD last_month_measure_tick = 0;
static DWORD last_backlog_event_tick = 0;
static int sim_accumulator_ms = 0;
static int actual_ms_per_month = 0;

void game_loop_reset(void) {
    last_frame_tick = GetTickCount();
    last_month_measure_tick = last_frame_tick;
    last_backlog_event_tick = 0;
    sim_accumulator_ms = 0;
    actual_ms_per_month = 0;
    sim_scheduler_reset();
    profiler_reset();
}

static int sim_budget_ms_for_speed(void) {
    if (!auto_run) return 3;
    if (speed_index <= 0) return 3;
    if (speed_index == 1) return 6;
    return 10;
}

static void record_completed_months(DWORD now) {
    int completed = sim_scheduler_take_completed_months();
    int elapsed;
    int sample;

    if (completed <= 0) return;
    elapsed = clamp((int)(now - last_month_measure_tick), 1, 30000);
    last_month_measure_tick = now;
    sample = elapsed / completed;
    actual_ms_per_month = actual_ms_per_month <= 0 ? sample :
                          (actual_ms_per_month * 3 + sample) / 4;
}

int game_loop_tick_frame(void) {
    DWORD now = GetTickCount();
    int elapsed;
    int did_work = 0;
    int did_visual = 0;
    int requested = 0;
    int budget_ms = sim_budget_ms_for_speed();
    int sim_used_ms = 0;

    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    if (auto_run) {
        int interval = SPEED_MS[clamp(speed_index, 0, 2)];
        int request_interval = interval;
        if (actual_ms_per_month > interval * 2) request_interval = min(actual_ms_per_month, 5000);
        sim_accumulator_ms += elapsed;
        if ((actual_ms_per_month > interval * 2 && sim_scheduler_pending_months() > 0) ||
            sim_scheduler_pending_months() >= sim_scheduler_pending_month_cap() - 1) {
            sim_scheduler_trim_pending_months(actual_ms_per_month > interval * 2 ? 1 : 2);
            if (now - last_backlog_event_tick > 2000) {
                event_log_push("[Performance] Simulation overloaded; throttled new month requests.");
                last_backlog_event_tick = now;
            }
            sim_accumulator_ms = min(sim_accumulator_ms, request_interval - 1);
        }
        while (sim_accumulator_ms >= request_interval && requested < 12) {
            if (!sim_scheduler_can_accept_month()) {
                if (now - last_backlog_event_tick > 2000) {
                    event_log_push("[Performance] Simulation month queue full; skipped new month request.");
                    last_backlog_event_tick = now;
                }
                sim_accumulator_ms = min(sim_accumulator_ms, request_interval - 1);
                break;
            }
            sim_accumulator_ms -= request_interval;
            sim_scheduler_request_month();
            requested++;
        }
        if (requested >= 12 && sim_accumulator_ms > request_interval) sim_accumulator_ms = request_interval;
    } else {
        sim_accumulator_ms = 0;
    }

    if (sim_scheduler_has_pending_work()) {
        DWORD start = GetTickCount();
        did_work |= sim_scheduler_run_for_ms(budget_ms);
        sim_used_ms = (int)(GetTickCount() - start);
    }
    record_completed_months(GetTickCount());
    did_visual = plague_visual_tick(elapsed);
    profiler_record_frame(elapsed, budget_ms, sim_used_ms, actual_ms_per_month,
                          game_loop_pending_months(), game_loop_simulation_overloaded());
    return did_work || did_visual || map_interaction_preview || dirty_any_render();
}

int game_loop_actual_ms_per_month(void) {
    return actual_ms_per_month;
}

int game_loop_pending_months(void) {
    return sim_scheduler_pending_months();
}

int game_loop_simulation_overloaded(void) {
    int target = SPEED_MS[clamp(speed_index, 0, 2)];
    return game_loop_pending_months() >= sim_scheduler_pending_month_cap() ||
           (actual_ms_per_month > 0 && actual_ms_per_month > target * 2 &&
            game_loop_pending_months() > 0);
}
