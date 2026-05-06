#include "ui_worldgen_layout.h"

#include "core/constants.h"

#include <string.h>

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

static RECT make_rect(int left, int top, int right, int bottom) {
    RECT rect = {left, top, right, bottom};
    return rect;
}

static RECT offset_rect_y(RECT rect, int dy) {
    rect.top += dy;
    rect.bottom += dy;
    return rect;
}

static void place_section(RECT *out, int x, int width, int *y, int scroll) {
    *out = offset_rect_y(make_rect(x, *y, x + width, *y + 26), -scroll);
    *y += 32;
}

static void place_slider(WorldgenSliderLayout *out, int x, int width, int *y, int scroll) {
    RECT label = make_rect(x, *y, x + width - 58, *y + 18);
    RECT value = make_rect(x + width - 50, *y, x + width, *y + 18);
    RECT track = make_rect(x, *y + 24, x + width, *y + 34);
    RECT hit = make_rect(x - 8, *y - 4, x + width + 8, *y + 42);

    out->label = offset_rect_y(label, -scroll);
    out->value = offset_rect_y(value, -scroll);
    out->track = offset_rect_y(track, -scroll);
    out->hit = offset_rect_y(hit, -scroll);
    *y += 44;
}

static void place_metric(WorldgenLayout *layout, int idx, int x, int y, int col_w, int scroll) {
    RECT label = make_rect(x, y, x + col_w - 6, y + 18);
    RECT input = make_rect(x, y + 20, x + col_w - 16, y + 44);

    layout->metric_label[idx] = offset_rect_y(label, -scroll);
    layout->metric_input[idx] = offset_rect_y(input, -scroll);
}

static int raw_content_height(RECT client, int panel_width) {
    WorldgenLayout layout;
    worldgen_layout_build(client, panel_width, 0, &layout);
    return layout.content_height;
}

void worldgen_layout_build(RECT client, int panel_width, int scroll_offset, WorldgenLayout *layout) {
    int panel_x = client.right - panel_width + FORM_X_PAD;
    int width = panel_width - FORM_X_PAD * 2;
    int y = TOP_BAR_H + 62;
    int gap = 8;
    int button_w = (width - gap * 2) / 3;
    int command_gap = 12;
    int command_w = (width - command_gap) / 2;
    int col_w = width / 4;
    int bottom_margin = 18;
    int i;

    memset(layout, 0, sizeof(*layout));
    layout->viewport = make_rect(panel_x, TOP_BAR_H + 52, client.right - FORM_X_PAD, client.bottom - bottom_margin);
    layout->scroll_offset = scroll_offset;

    layout->title = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 28), -scroll_offset);
    y += 36;
    place_section(&layout->map_size_section, panel_x, width, &y, scroll_offset);
    for (i = 0; i < MAP_SIZE_COUNT; i++) {
        RECT button = make_rect(panel_x + i * (button_w + gap), y, panel_x + i * (button_w + gap) + button_w, y + 28);
        layout->map_size_buttons[i] = offset_rect_y(button, -scroll_offset);
    }
    y += 44;
    layout->initial_label = offset_rect_y(make_rect(panel_x, y, panel_x + width - 86, y + 24), -scroll_offset);
    layout->initial_input = offset_rect_y(make_rect(panel_x + width - 76, y - 2, panel_x + width, y + 22), -scroll_offset);
    y += 40;

    place_section(&layout->physical_section, panel_x, width, &y, scroll_offset);
    for (i = WORLD_SLIDER_OCEAN; i <= WORLD_SLIDER_VEGETATION; i++) {
        place_slider(&layout->sliders[i], panel_x, width, &y, scroll_offset);
    }
    y += 4;
    place_section(&layout->terrain_section, panel_x, width, &y, scroll_offset);
    for (i = WORLD_SLIDER_BIAS_FOREST; i <= WORLD_SLIDER_BIAS_WETLAND; i++) {
        place_slider(&layout->sliders[i], panel_x, width, &y, scroll_offset);
    }
    y += 4;
    place_section(&layout->regions_section, panel_x, width, &y, scroll_offset);
    place_slider(&layout->sliders[UI_SLIDER_REGION_SIZE], panel_x, width, &y, scroll_offset);
    layout->generate_row = offset_rect_y(make_rect(panel_x, y + 2, panel_x + width, y + 24), -scroll_offset);
    y += 36;

    place_section(&layout->civ_section, panel_x, width, &y, scroll_offset);
    layout->civ_hint = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 20), -scroll_offset);
    y += 24;
    layout->name_label = offset_rect_y(make_rect(panel_x, y, panel_x + 160, y + 18), -scroll_offset);
    layout->symbol_label = offset_rect_y(make_rect(panel_x + 176, y, panel_x + 238, y + 18), -scroll_offset);
    layout->name_input = offset_rect_y(make_rect(panel_x, y + 20, panel_x + 160, y + 44), -scroll_offset);
    layout->symbol_input = offset_rect_y(make_rect(panel_x + 176, y + 20, panel_x + 226, y + 44), -scroll_offset);
    y += 52;
    place_metric(layout, WORLDGEN_METRIC_MILITARY, panel_x, y, col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_LOGISTICS, panel_x + col_w, y, col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_GOVERNANCE, panel_x + col_w * 2, y, col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_COHESION, panel_x + col_w * 3, y, col_w, scroll_offset);
    y += 50;
    place_metric(layout, WORLDGEN_METRIC_PRODUCTION, panel_x, y, col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_COMMERCE, panel_x + col_w, y, col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_INNOVATION, panel_x + col_w * 2, y, col_w, scroll_offset);
    y += 52;
    layout->add_button = offset_rect_y(make_rect(panel_x, y, panel_x + command_w, y + 30), -scroll_offset);
    layout->apply_button = offset_rect_y(make_rect(panel_x + command_w + command_gap, y, panel_x + width, y + 30), -scroll_offset);
    y += 42;

    layout->content_height = y - (TOP_BAR_H + 62);
    layout->max_scroll = max_int(0, layout->content_height - (layout->viewport.bottom - layout->viewport.top));
}

int worldgen_layout_clamp_scroll(RECT client, int panel_width, int scroll_offset) {
    int content_height = raw_content_height(client, panel_width);
    int viewport_height = client.bottom - 18 - (TOP_BAR_H + 52);
    int max_scroll = max_int(0, content_height - viewport_height);
    return clamp_int(scroll_offset, 0, max_scroll);
}

int worldgen_rect_visible(RECT viewport, RECT rect) {
    return rect.right > viewport.left && rect.left < viewport.right &&
           rect.bottom > viewport.top && rect.top < viewport.bottom;
}
