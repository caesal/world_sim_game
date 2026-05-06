#ifndef WORLD_SIM_GAME_H
#define WORLD_SIM_GAME_H

int run_game(void);
void game_toggle_auto_run(void);
void game_request_new_world(void);
void game_request_regenerate_regions(void);
int game_request_add_civilization_from_selection(const char *name, char symbol,
                                                int military, int logistics,
                                                int governance, int cohesion,
                                                int production, int commerce,
                                                int innovation);
int game_request_edit_selected_civilization(const char *name, char symbol,
                                            int military, int logistics,
                                            int governance, int cohesion,
                                            int production, int commerce,
                                            int innovation);
int game_tick_auto_run(void);

#endif
