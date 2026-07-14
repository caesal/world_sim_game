#ifndef WORLD_SIM_UI_PLAGUE_FOG_H
#define WORLD_SIM_UI_PLAGUE_FOG_H

#include <stdint.h>
#include <windows.h>

#define PLAGUE_FOG_DEFAULT_PERCENT 50

typedef struct {
    RECT label;
    RECT value;
    RECT track;
    RECT help;
    RECT hit;
} PlagueFogSliderLayout;

typedef struct {
    int title_top;
    RECT effect_help;
    int effect_bottom;
    PlagueFogSliderLayout slider;
    int content_top;
} PlaguePanelLayout;

void ui_plague_fog_layout_build(RECT client, int panel_width,
                                PlaguePanelLayout *layout);
int ui_plague_fog_percent(int stored_value);
int ui_plague_fog_effective_strength(int stored_value);
uint32_t ui_plague_fog_scale_premultiplied(uint32_t pixel,
                                           int effective_strength);

#endif
