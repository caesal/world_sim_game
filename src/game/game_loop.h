#ifndef WORLD_SIM_GAME_LOOP_H
#define WORLD_SIM_GAME_LOOP_H

void game_loop_reset(void);

enum {
    GAME_REDRAW_NONE = 0,
    GAME_REDRAW_TOP_BAR = 1 << 0,
    GAME_REDRAW_MAP_DYNAMIC = 1 << 1,
    GAME_REDRAW_MAP_STATIC = 1 << 2,
    GAME_REDRAW_SIDE_PANEL = 1 << 3,
    GAME_REDRAW_BOTTOM_BAR = 1 << 4,
    GAME_REDRAW_FULL = 1 << 5,
    GAME_REDRAW_PLAGUE_OVERLAY = 1 << 6,
    GAME_REDRAW_SIDE_PANEL_DATA = 1 << 7
};

int game_loop_tick_frame(void);
int game_loop_actual_ms_per_month(void);
int game_loop_pending_months(void);
int game_loop_simulation_overloaded(void);
int game_loop_snapshot_age_ms(void);
int game_loop_presentation_backlog(void);
int game_loop_visual_coalesced_months(void);
int game_loop_visual_max_backlog(void);
int game_loop_visual_presented_total(void);
int game_loop_visual_dropped_months(void);
int game_loop_displayed_month_order_skips(void);
int game_loop_presentation_throttled(void);
const char *game_loop_worker_status(void);
int game_loop_last_redraw_flags(void);
int game_loop_last_completed_months(void);
int game_loop_display_year(void);
int game_loop_display_month(void);
int game_loop_last_completed_month_map_redraw(void);
const char *game_loop_last_map_redraw_reason(void);

#endif
