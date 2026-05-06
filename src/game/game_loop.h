#ifndef WORLD_SIM_GAME_LOOP_H
#define WORLD_SIM_GAME_LOOP_H

void game_loop_reset(void);
int game_loop_tick_frame(void);
int game_loop_actual_ms_per_month(void);
int game_loop_pending_months(void);

#endif
