#ifndef WORLD_SIM_UI_CLAY_PRIMITIVES_H
#define WORLD_SIM_UI_CLAY_PRIMITIVES_H

#include "ui/ui_clay_theme.h"

void ui_clay_draw_shell(HDC hdc, RECT rect);
void ui_clay_draw_panel(HDC hdc, RECT rect, UiClayState state);
void ui_clay_draw_card(HDC hdc, RECT rect, UiClayState state);
void ui_clay_draw_pill(HDC hdc, RECT rect, UiClayState state);
void ui_clay_draw_pill_inset(HDC hdc, RECT rect, UiClayState state);
void ui_clay_draw_tab(HDC hdc, RECT rect, UiClayState state);

#endif
