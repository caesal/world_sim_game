#ifndef WORLD_SIM_UI_CLAY_THEME_H
#define WORLD_SIM_UI_CLAY_THEME_H

#include <windows.h>

typedef enum {
    UI_CLAY_STATE_NORMAL,
    UI_CLAY_STATE_HOVER,
    UI_CLAY_STATE_PRESSED,
    UI_CLAY_STATE_SELECTED,
    UI_CLAY_STATE_DISABLED
} UiClayState;

typedef enum {
    UI_CLAY_SURFACE_SHELL,
    UI_CLAY_SURFACE_PANEL,
    UI_CLAY_SURFACE_CARD,
    UI_CLAY_SURFACE_PILL,
    UI_CLAY_SURFACE_TAB
} UiClaySurface;

typedef struct {
    COLORREF fill;
    COLORREF border;
    COLORREF shadow;
    COLORREF highlight;
    COLORREF text;
    COLORREF text_muted;
    int radius;
    int padding_x;
    int padding_y;
    int shadow_offset;
} UiClayStyle;

UiClayStyle ui_clay_style(UiClaySurface surface, UiClayState state);
COLORREF ui_clay_text_color(UiClayState state);
COLORREF ui_clay_muted_text_color(void);

#endif
