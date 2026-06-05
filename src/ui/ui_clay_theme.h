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

typedef enum {
    UI_CLAY_TONE_NEUTRAL,
    UI_CLAY_TONE_PEACE,
    UI_CLAY_TONE_TENSE,
    UI_CLAY_TONE_TRUCE,
    UI_CLAY_TONE_WAR,
    UI_CLAY_TONE_TRIBUTE,
    UI_CLAY_TONE_VASSAL,
    UI_CLAY_TONE_MUTED
} UiClaySemanticTone;

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

typedef struct {
    COLORREF accent;
    COLORREF tag_fill;
    COLORREF tag_text;
    COLORREF soft_fill;
    COLORREF border;
} UiClaySemanticStyle;

UiClayStyle ui_clay_style(UiClaySurface surface, UiClayState state);
UiClaySemanticStyle ui_clay_semantic_style(UiClaySemanticTone tone);
COLORREF ui_clay_text_color(UiClayState state);
COLORREF ui_clay_muted_text_color(void);

#endif
