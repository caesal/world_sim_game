#ifndef WORLD_SIM_UI_THEME_H
#define WORLD_SIM_UI_THEME_H

#include "ui/ui_types.h"

typedef enum {
    UI_COLOR_CHROME,
    UI_COLOR_PANEL,
    UI_COLOR_PANEL_SOFT,
    UI_COLOR_PANEL_LINE,
    UI_COLOR_TEXT,
    UI_COLOR_TEXT_MUTED,
    UI_COLOR_TEXT_DIM,
    UI_COLOR_ACCENT,
    UI_COLOR_ACCENT_SOFT,
    UI_COLOR_DANGER,
    UI_COLOR_GOOD
} UiThemeColor;

COLORREF ui_theme_color(UiThemeColor color);

#endif
