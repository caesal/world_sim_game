#include "ui_sliders.h"

#include "core/dirty_flags.h"
#include "game/game.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"

int divider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int divider_x;

    GetClientRect(hwnd, &client);
    divider_x = client.right - side_panel_w;
    return mouse_y >= TOP_BAR_H && mouse_x >= divider_x - 6 && mouse_x <= divider_x + 6;
}

static WorldgenSliderLayout plague_slider_layout(RECT client) {
    WorldgenSliderLayout slider;
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2 - 8;
    int y = TOP_BAR_H + 154;

    slider.label = (RECT){x, y, x + width - 58, y + 18};
    slider.value = (RECT){x + width - 50, y, x + width, y + 18};
    slider.track = (RECT){x, y + 24, x + width, y + 34};
    slider.hit = (RECT){x - 8, y - 4, x + width + 8, y + 42};
    return slider;
}

static WorldgenSliderLayout slider_layout(HWND hwnd, int index) {
    RECT client;
    WorldgenLayout layout;

    GetClientRect(hwnd, &client);
    if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) return plague_slider_layout(client);
    worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
    return layout.sliders[index];
}

RECT setup_slider_rect(HWND hwnd, int index) {
    return slider_layout(hwnd, index).track;
}

int setup_slider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    int i;

    if (panel_tab != PANEL_WORLD && panel_tab != PANEL_PLAGUE) return -1;
    for (i = 0; i < UI_SLIDER_COUNT; i++) {
        if (panel_tab == PANEL_PLAGUE && i != UI_SLIDER_PLAGUE_FOG_ALPHA) continue;
        if (panel_tab == PANEL_WORLD && i == UI_SLIDER_PLAGUE_FOG_ALPHA) continue;
        WorldgenSliderLayout slider = slider_layout(hwnd, i);
        if (point_in_rect(slider.hit, mouse_x, mouse_y)) return i;
    }
    return -1;
}

void update_setup_slider(HWND hwnd, int index, int mouse_x) {
    RECT track = setup_slider_rect(hwnd, index);
    int value = clamp((mouse_x - track.left) * 100 / (track.right - track.left), 0, 100);

    if (index == WORLD_SLIDER_OCEAN) ocean_slider = value;
    else if (index == WORLD_SLIDER_CONTINENT) continent_slider = value;
    else if (index == WORLD_SLIDER_RELIEF) relief_slider = value;
    else if (index == WORLD_SLIDER_MOISTURE) moisture_slider = value;
    else if (index == WORLD_SLIDER_DROUGHT) drought_slider = value;
    else if (index == WORLD_SLIDER_VEGETATION) vegetation_slider = value;
    else if (index == WORLD_SLIDER_BIAS_FOREST) bias_forest_slider = value;
    else if (index == WORLD_SLIDER_BIAS_DESERT) bias_desert_slider = value;
    else if (index == WORLD_SLIDER_BIAS_MOUNTAIN) bias_mountain_slider = value;
    else if (index == WORLD_SLIDER_BIAS_WETLAND) bias_wetland_slider = value;
    else if (index == UI_SLIDER_REGION_SIZE) {
        if (region_size_slider == value) return;
        region_size_slider = value;
        game_request_regenerate_regions();
    }
    else if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) {
        if (plague_fog_alpha == value) return;
        plague_fog_alpha = value;
        dirty_mark_plague();
    }
    InvalidateRect(hwnd, NULL, FALSE);
}
