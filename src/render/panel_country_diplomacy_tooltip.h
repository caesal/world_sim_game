#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_TOOLTIP_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_TOOLTIP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

void diplomacy_score_tooltip_begin(void);
void diplomacy_score_tooltip_register_bar(RECT rect, int civ_id, int other_id);
int diplomacy_score_tooltip_registered_count(void);
int diplomacy_score_tooltip_hit_test(int mouse_x, int mouse_y, int *civ_id, int *other_id);
int diplomacy_score_tooltip_hover_key(int mouse_x, int mouse_y);
void diplomacy_score_tooltip_draw(HDC hdc, RECT bounds);
int diplomacy_score_tooltip_net_for_relation(int civ_id, int other_id,
                                             int *factor_sum_x100,
                                             int *other_x100,
                                             int *display_x100);
void diplomacy_score_tooltip_format_delta(int delta_x100, char *out, size_t size);

#endif
