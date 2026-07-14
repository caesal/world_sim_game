#include "ui_sliders.h"

#include "game/game.h"
#include "game/game_loop.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"

int divider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int divider_x;

    if (side_panel_collapsed) return 0;
    GetClientRect(hwnd, &client);
    divider_x = client.right - side_panel_w;
    return mouse_y >= TOP_BAR_H && mouse_x >= divider_x - 6 && mouse_x <= divider_x + 6;
}

static WorldgenSliderLayout plague_slider_layout(RECT client) {
    PlaguePanelLayout plague_layout;
    WorldgenSliderLayout slider;
    ui_plague_fog_layout_build(client, side_panel_w, &plague_layout);
    slider.label = plague_layout.slider.label;
    slider.value = plague_layout.slider.value;
    slider.track = plague_layout.slider.track;
    slider.help = plague_layout.slider.help;
    slider.hit = plague_layout.slider.hit;
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

    if (side_panel_collapsed) return -1;
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
        if (ui_plague_fog_percent(plague_fog_alpha) == value) return;
        plague_fog_alpha = value;
    }
    if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) {
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_PLAGUE_OVERLAY | GAME_REDRAW_SIDE_PANEL);
    } else if (index == UI_SLIDER_REGION_SIZE) {
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
    } else {
        ui_invalidate_side_panel(hwnd);
    }
}
