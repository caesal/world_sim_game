#include "ui_theme.h"

COLORREF ui_theme_color(UiThemeColor color) {
    switch (color) {
        case UI_COLOR_CHROME: return RGB(28, 32, 34);
        case UI_COLOR_PANEL: return RGB(34, 39, 42);
        case UI_COLOR_PANEL_SOFT: return RGB(43, 49, 52);
        case UI_COLOR_PANEL_LINE: return RGB(72, 80, 82);
        case UI_COLOR_TEXT: return RGB(236, 232, 221);
        case UI_COLOR_TEXT_MUTED: return RGB(188, 194, 190);
        case UI_COLOR_TEXT_DIM: return RGB(138, 148, 148);
        case UI_COLOR_ACCENT: return RGB(188, 154, 88);
        case UI_COLOR_ACCENT_SOFT: return RGB(96, 112, 102);
        case UI_COLOR_DANGER: return RGB(169, 78, 67);
        case UI_COLOR_GOOD: return RGB(92, 145, 98);
        default: return RGB(236, 232, 221);
    }
}
