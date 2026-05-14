#ifndef WORLD_SIM_GAME_H
#define WORLD_SIM_GAME_H

#include "core/value_types.h"

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
void game_request_set_civilization_color(int civ_id, Color32 color);
void game_request_after_load_map(void);
int game_request_trigger_civil_unrest(int civ_id);
int game_request_release_vassal(int vassal_id);
int game_tick_auto_run(void);
int run_expansion_probe(void);
int run_tech10_probe(void);

#endif
