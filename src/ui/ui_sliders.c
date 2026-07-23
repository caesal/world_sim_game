#include "ui_sliders.h"

#include "game/game_loop.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_forms.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_view.h"

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
    UiWorldgenPanelLayout panel_layout;

    GetClientRect(hwnd, &client);
    if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) return plague_slider_layout(client);
    ui_worldgen_view_build(client, side_panel_w, &panel_layout);
    ui_worldgen_view_build_legacy_layout(&panel_layout.legacy, &layout);
    return layout.sliders[index];
}

static int point_in_worldgen_legacy_viewport(HWND hwnd, int x, int y) {
    RECT client;
    UiWorldgenPanelLayout layout;
    GetClientRect(hwnd, &client);
    ui_worldgen_view_build(client, side_panel_w, &layout);
    return ui_worldgen_panel_point_in_viewport(layout.legacy.viewport, x, y);
}

RECT setup_slider_rect(HWND hwnd, int index) {
    return slider_layout(hwnd, index).track;
}

int setup_slider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    int i;

    if (side_panel_collapsed) return -1;
    if (panel_tab != PANEL_WORLD && panel_tab != PANEL_PLAGUE) return -1;
    if (panel_tab == PANEL_WORLD &&
        ui_worldgen_control_state_get()->tab != UI_WORLDGEN_TAB_LEGACY) return -1;
    if (panel_tab == PANEL_WORLD &&
        !point_in_worldgen_legacy_viewport(hwnd, mouse_x, mouse_y)) return -1;
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
    UiWorldgenConfigField field = UI_WORLDGEN_FIELD_COUNT;

    if (index == WORLD_SLIDER_OCEAN) field = UI_WORLDGEN_FIELD_OCEAN;
    else if (index == WORLD_SLIDER_CONTINENT) field = UI_WORLDGEN_FIELD_CONTINENT;
    else if (index == WORLD_SLIDER_RELIEF) field = UI_WORLDGEN_FIELD_RELIEF;
    else if (index == WORLD_SLIDER_MOISTURE) field = UI_WORLDGEN_FIELD_MOISTURE;
    else if (index == WORLD_SLIDER_DROUGHT) field = UI_WORLDGEN_FIELD_DROUGHT;
    else if (index == WORLD_SLIDER_VEGETATION) field = UI_WORLDGEN_FIELD_VEGETATION;
    else if (index == WORLD_SLIDER_BIAS_FOREST) field = UI_WORLDGEN_FIELD_BIAS_FOREST;
    else if (index == WORLD_SLIDER_BIAS_DESERT) field = UI_WORLDGEN_FIELD_BIAS_DESERT;
    else if (index == WORLD_SLIDER_BIAS_MOUNTAIN) field = UI_WORLDGEN_FIELD_BIAS_MOUNTAIN;
    else if (index == WORLD_SLIDER_BIAS_WETLAND) field = UI_WORLDGEN_FIELD_BIAS_WETLAND;
    else if (index == UI_SLIDER_REGION_SIZE) field = UI_WORLDGEN_FIELD_REGION_SIZE;
    else if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) {
        if (ui_plague_fog_percent(plague_fog_alpha) == value) return;
        plague_fog_alpha = value;
    }
    if (field != UI_WORLDGEN_FIELD_COUNT &&
        !ui_worldgen_control_state_set_field(field, value)) return;
    if (field != UI_WORLDGEN_FIELD_COUNT) ui_forms_write_world_setup_controls();
    if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) {
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_PLAGUE_OVERLAY | GAME_REDRAW_SIDE_PANEL);
    } else {
        ui_invalidate_side_panel(hwnd);
    }
}
