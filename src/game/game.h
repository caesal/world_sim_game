#ifndef WORLD_SIM_GAME_H
#define WORLD_SIM_GAME_H

#include "core/value_types.h"
#include "game/game_plague_baseline_probe.h"
#include "game/game_plague_probe.h"
#include "game/game_plague_performance_probe.h"

int run_game(void);
int run_game_no_activate(void);
void game_toggle_auto_run(void);
void game_request_pause(void);
void game_pause_for_modal_or_action(void);
void game_request_new_world(void);
void game_request_regenerate_regions(void);
int game_request_add_civilization_from_selection(const char *name, char symbol,
                                                int military, int logistics,
                                                int governance, int cohesion,
                                                int production, int commerce,
                                                int innovation);
int game_request_add_civilization_from_selection_with_color(const char *name, char symbol,
                                                           int military, int logistics,
                                                           int governance, int cohesion,
                                                           int production, int commerce,
                                                           int innovation,
                                                           Color32 color);
int game_request_edit_selected_civilization(const char *name, char symbol,
                                            int military, int logistics,
                                            int governance, int cohesion,
                                            int production, int commerce,
                                            int innovation);
void game_request_set_civilization_color(int civ_id, Color32 color);
void game_request_set_civilization_color_exact(int civ_id, Color32 color);
void game_request_set_civilization_color_auto_avoid(int civ_id, Color32 preferred_color);
Color32 game_preview_civilization_color_auto_avoid(int civ_id, Color32 preferred_color);
void game_request_after_load_map(int restored_dynamic_state);
int game_request_trigger_civil_unrest(int civ_id);
int game_request_release_vassal(int vassal_id);
int game_request_annex_vassal(int overlord_id, int vassal_id);
int game_tick_auto_run(void);
int run_expansion_probe(void);
int run_tech10_probe(void);
int run_economy_probe(void);
int run_collapse_color_probe(void);
int run_crisis_probe(void);
int run_diplomacy_probe(void);
int run_population_probe(void);
int run_presentation_probe(void);
int run_worldgen_probe(void);
int run_expansion_perf_probe(void);
int run_military_alliance_probe(void);

#endif
