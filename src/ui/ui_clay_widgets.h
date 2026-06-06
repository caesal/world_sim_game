#ifndef WORLD_SIM_UI_CLAY_WIDGETS_H
#define WORLD_SIM_UI_CLAY_WIDGETS_H

#include "ui/ui_clay_theme.h"

UiClayState ui_clay_state_from_flags(int hovered, int pressed, int selected, int disabled);
UiClayState ui_clay_state_for_rect(RECT rect, int x, int y, int selected, int disabled);

void ui_clay_draw_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_pill_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_icon_button(HDC hdc, RECT rect, const char *label, UiClayState state);
void ui_clay_draw_menu_panel(HDC hdc, RECT rect);
void ui_clay_draw_section_header(HDC hdc, RECT rect, const char *label);
void ui_clay_draw_input_frame(HDC hdc, RECT rect, UiClayState state);
void ui_clay_draw_slider(HDC hdc, RECT track, int value, UiClayState state);
void ui_clay_draw_progress_bar(HDC hdc, RECT rect, int value, int max_value, COLORREF color);
void ui_clay_draw_swatch(HDC hdc, RECT rect, COLORREF color, UiClayState state);
void ui_clay_draw_metric_chip_text(HDC hdc, RECT rect, int icon, const char *label,
                                   const char *value, COLORREF accent);
void ui_clay_draw_metric_chip_int(HDC hdc, RECT rect, int icon, const char *label,
                                  int value, COLORREF accent);

#endif
