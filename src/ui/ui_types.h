#ifndef WORLD_SIM_UI_TYPES_H
#define WORLD_SIM_UI_TYPES_H

#include "core/game_types.h"

#define ID_NAME_EDIT 101
#define ID_SYMBOL_EDIT 102
#define ID_MILITARY_EDIT 103
#define ID_LOGISTICS_EDIT 104
#define ID_GOVERNANCE_EDIT 105
#define ID_COHESION_EDIT 106
#define ID_PRODUCTION_EDIT 107
#define ID_COMMERCE_EDIT 108
#define ID_INNOVATION_EDIT 109
#define ID_ADD_BUTTON 110
#define ID_APPLY_BUTTON 111
#define ID_INITIAL_CIVS_EDIT 112

typedef enum {
    DISPLAY_OVERVIEW,
    DISPLAY_CLIMATE,
    DISPLAY_GEOGRAPHY,
    DISPLAY_REGIONS,
    DISPLAY_POLITICAL,
    DISPLAY_ALL
} DisplayMode;

typedef enum {
    PANEL_SELECTION,
    PANEL_COUNTRY,
    PANEL_DIPLOMACY,
    PANEL_POPULATION,
    PANEL_PLAGUE,
    PANEL_WORLD,
    PANEL_DEBUG
} PanelTab;

#define PANEL_TAB_COUNT 7

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
    WORLD_SLIDER_COUNT,
    UI_SLIDER_REGION_SIZE = WORLD_SLIDER_COUNT,
    UI_SLIDER_PLAGUE_FOG_ALPHA,
    UI_SLIDER_COUNT
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
    HWND military_edit;
    HWND logistics_edit;
    HWND governance_edit;
    HWND cohesion_edit;
    HWND production_edit;
    HWND commerce_edit;
    HWND innovation_edit;
    HWND initial_civs_edit;
    HWND add_button;
    HWND apply_button;
} FormControls;

#endif
