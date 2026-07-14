#include "ui/ui_plague_fog.h"

#include "core/constants.h"

#include <string.h>

enum {
    PLAGUE_TITLE_TOP_OFFSET = 62,
    PLAGUE_TITLE_ADVANCE = 30,
    PLAGUE_SECTION_ADVANCE = 31,
    PLAGUE_ROW_ADVANCE = 21,
    PLAGUE_EFFECT_ADVANCE = 50,
    PLAGUE_SLIDER_GAP = 9,
    PLAGUE_SLIDER_LABEL_HEIGHT = 18,
    PLAGUE_SLIDER_TRACK_TOP = 24,
    PLAGUE_SLIDER_TRACK_HEIGHT = 10,
    PLAGUE_SLIDER_HELP_TOP = 40,
    PLAGUE_SLIDER_HELP_HEIGHT = 18,
    PLAGUE_SLIDER_HIT_TOP_PAD = 4,
    PLAGUE_SLIDER_HIT_BOTTOM = 60,
    PLAGUE_CONTENT_GAP = 18
};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static unsigned int scale_channel(unsigned int channel, int strength) {
    uint64_t scaled = (uint64_t)channel * (uint64_t)strength;
    scaled = (scaled + 50u) / 100u;
    return (unsigned int)(scaled > 255u ? 255u : scaled);
}

void ui_plague_fog_layout_build(RECT client, int panel_width,
                                PlaguePanelLayout *layout) {
    PlagueFogSliderLayout *slider;
    int x;
    int width;
    int y;
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    layout->title_top = TOP_BAR_H + PLAGUE_TITLE_TOP_OFFSET;
    layout->effect_bottom = layout->title_top + PLAGUE_TITLE_ADVANCE +
        PLAGUE_SECTION_ADVANCE + PLAGUE_ROW_ADVANCE + PLAGUE_EFFECT_ADVANCE;
    x = client.right - panel_width + FORM_X_PAD;
    width = panel_width - FORM_X_PAD * 2 - 8;
    layout->effect_help = (RECT){x,
        layout->effect_bottom - PLAGUE_EFFECT_ADVANCE,
        x + width, layout->effect_bottom};
    y = layout->effect_bottom + PLAGUE_SLIDER_GAP;
    slider = &layout->slider;
    slider->label = (RECT){x, y, x + width - 58,
                           y + PLAGUE_SLIDER_LABEL_HEIGHT};
    slider->value = (RECT){x + width - 50, y, x + width,
                           y + PLAGUE_SLIDER_LABEL_HEIGHT};
    slider->track = (RECT){x, y + PLAGUE_SLIDER_TRACK_TOP, x + width,
                           y + PLAGUE_SLIDER_TRACK_TOP + PLAGUE_SLIDER_TRACK_HEIGHT};
    slider->help = (RECT){x, y + PLAGUE_SLIDER_HELP_TOP, x + width,
                          y + PLAGUE_SLIDER_HELP_TOP + PLAGUE_SLIDER_HELP_HEIGHT};
    slider->hit = (RECT){x - 8, y - PLAGUE_SLIDER_HIT_TOP_PAD,
                         x + width + 8, y + PLAGUE_SLIDER_HIT_BOTTOM};
    layout->content_top = slider->hit.bottom + PLAGUE_CONTENT_GAP;
}

int ui_plague_fog_percent(int stored_value) {
    return clamp_int(stored_value, 0, 100);
}

int ui_plague_fog_effective_strength(int stored_value) {
    int percent = ui_plague_fog_percent(stored_value);
    if (percent <= 50) return (percent * 80 + 25) / 50;
    return 80 + ((percent - 50) * 40 + 25) / 50;
}

uint32_t ui_plague_fog_scale_premultiplied(uint32_t pixel,
                                           int effective_strength) {
    unsigned int strength = (unsigned int)clamp_int(effective_strength, 0, 120);
    unsigned int blue = scale_channel(pixel & 0xffu, (int)strength);
    unsigned int green = scale_channel((pixel >> 8) & 0xffu, (int)strength);
    unsigned int red = scale_channel((pixel >> 16) & 0xffu, (int)strength);
    unsigned int alpha = scale_channel((pixel >> 24) & 0xffu, (int)strength);
    return blue | (green << 8) | (red << 16) | (alpha << 24);
}
