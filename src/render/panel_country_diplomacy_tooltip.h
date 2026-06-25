#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_TOOLTIP_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_TOOLTIP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

#define SCORE_TOOLTIP_SCOPE_NONE 0
#define SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY 1
#define SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES 2

void diplomacy_score_tooltip_begin(void);
void diplomacy_score_tooltip_begin_scope(int scope);
void diplomacy_score_tooltip_register_bar(RECT rect, int civ_id, int other_id);
void diplomacy_score_tooltip_commit(void);
void diplomacy_score_tooltip_commit_scope(int scope);
int diplomacy_score_tooltip_registered_count(void);
int diplomacy_score_tooltip_build_count(void);
int diplomacy_score_tooltip_active_scope(void);
int diplomacy_score_tooltip_hit_test(int mouse_x, int mouse_y, int *civ_id, int *other_id);
int diplomacy_score_tooltip_hover_key(int mouse_x, int mouse_y);
int diplomacy_score_tooltip_hover_key_for_scope(int scope, int mouse_x, int mouse_y);
void diplomacy_score_tooltip_draw(HDC hdc, RECT bounds);
int diplomacy_score_tooltip_net_for_relation(int civ_id, int other_id,
                                             int *factor_sum_x100,
                                             int *other_x100,
                                             int *display_x100);
void diplomacy_score_tooltip_format_delta(int delta_x100, char *out, size_t size);

#endif
