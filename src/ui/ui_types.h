#ifndef WORLD_SIM_UI_TYPES_H
#define WORLD_SIM_UI_TYPES_H

#include "core/game_types.h"

#define ID_NAME_EDIT 101
#define ID_SYMBOL_EDIT 102
#define ID_AGGRESSION_EDIT 103
#define ID_EXPANSION_EDIT 104
#define ID_DEFENSE_EDIT 105
#define ID_CULTURE_EDIT 106
#define ID_ADD_BUTTON 107
#define ID_APPLY_BUTTON 108
#define ID_INITIAL_CIVS_EDIT 109

typedef enum {
    DISPLAY_OVERVIEW,
    DISPLAY_CLIMATE,
    DISPLAY_GEOGRAPHY,
    DISPLAY_POLITICAL,
    DISPLAY_ALL
} DisplayMode;

typedef enum {
    PANEL_INFO,
    PANEL_CIV,
    PANEL_DIPLOMACY,
    PANEL_MAP
} PanelTab;

#define PANEL_TAB_COUNT 4

typedef enum {
    WORLD_SLIDER_OCEAN,
    WORLD_SLIDER_CONTINENT,
    WORLD_SLIDER_RELIEF,
    WORLD_SLIDER_MOISTURE,
    WORLD_SLIDER_DROUGHT,
    WORLD_SLIDER_VEGETATION,
    WORLD_SLIDER_BIAS_FOREST,
    WORLD_SLIDER_BIAS_DESERT,
    WORLD_SLIDER_BIAS_MOUNTAIN,
    WORLD_SLIDER_BIAS_WETLAND,
    WORLD_SLIDER_COUNT
} WorldSetupSlider;

typedef enum {
    UI_LANG_EN,
    UI_LANG_ZH
} UiLanguage;

typedef struct {
    int map_x;
    int map_y;
    int tile_size;
    int draw_w;
    int draw_h;
} MapLayout;

typedef struct {
    HWND name_edit;
    HWND symbol_edit;
    HWND aggression_edit;
    HWND expansion_edit;
    HWND defense_edit;
    HWND culture_edit;
    HWND initial_civs_edit;
    HWND add_button;
    HWND apply_button;
} FormControls;

#endif
