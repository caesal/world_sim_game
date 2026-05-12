#ifndef WORLD_SIM_UI_SELECTION_H
#define WORLD_SIM_UI_SELECTION_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum {
    UI_SELECT_SOURCE_COUNTRY_LIST,
    UI_SELECT_SOURCE_MAP,
    UI_SELECT_SOURCE_DIPLOMACY_CARD
} UiSelectSource;

typedef enum {
    UI_HIGHLIGHT_SOURCE_EVENT_LOG
} UiHighlightSource;

void ui_select_civ(int civ_id, UiSelectSource source);
void ui_select_civ_preserve_view(int civ_id, UiSelectSource source);
void ui_clear_selected_civ(UiSelectSource source);
void ui_highlight_civ(int civ_id, UiHighlightSource source);
void ui_clear_map_highlight(void);
int ui_locate_civ(HWND hwnd, int civ_id);
int ui_highlight_pulse_active(void);

#endif
