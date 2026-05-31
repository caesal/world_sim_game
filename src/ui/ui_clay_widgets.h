#ifndef WORLD_SIM_UI_CLAY_WIDGETS_H
#define WORLD_SIM_UI_CLAY_WIDGETS_H

#include "ui/ui_clay_theme.h"

UiClayState ui_clay_state_from_flags(int hovered, int pressed, int selected, int disabled);
UiClayState ui_clay_state_for_rect(RECT rect, int x, int y, int selected, int disabled);

void ui_clay_draw_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_pill_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_icon_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_menu_panel(HDC hdc, RECT rect);

#endif
