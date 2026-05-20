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
    GAME_REDRAW_FULL = 1 << 5
};

int game_loop_tick_frame(void);
int game_loop_actual_ms_per_month(void);
int game_loop_pending_months(void);
int game_loop_simulation_overloaded(void);
int game_loop_snapshot_age_ms(void);
const char *game_loop_worker_status(void);

#endif
