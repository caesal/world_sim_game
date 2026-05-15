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

static RECT inset_rect(RECT rect, int dx, int dy) {
    rect.left += dx;
    rect.right -= dx;
    rect.top += dy;
    rect.bottom -= dy;
    return rect;
}

static void place_section(RECT *out, int x, int width, int *y, int scroll) {
    *out = offset_rect_y(make_rect(x, *y, x + width, *y + 26), -scroll);
    *y += 32;
}

static RECT section_button(RECT section) {
    return make_rect(section.right - 78, section.top + 1, section.right, section.top + 25);
}

static void place_slider(WorldgenSliderLayout *out, int x, int width, int *y, int scroll) {
    RECT label = make_rect(x, *y, x + width - 106, *y + 18);
    RECT value = make_rect(x + width - 98, *y, x + width, *y + 18);
    RECT track = make_rect(x, *y + 24, x + width, *y + 34);
    RECT help = make_rect(x, *y + 40, x + width, *y + 58);
    RECT hit = make_rect(x - 8, *y - 4, x + width + 8, *y + 60);

    out->label = offset_rect_y(label, -scroll);
    out->value = offset_rect_y(value, -scroll);
    out->track = offset_rect_y(track, -scroll);
    out->help = offset_rect_y(help, -scroll);
    out->hit = offset_rect_y(hit, -scroll);
    *y += 66;
}

static void place_metric(WorldgenLayout *layout, int idx, int x, int y, int col_w, int scroll) {
    RECT label = make_rect(x, y, x + col_w - 6, y + 18);
    RECT frame = make_rect(x, y + 22, x + col_w - 16, y + 52);
    RECT input = inset_rect(frame, 5, 3);
    RECT help = make_rect(x, y + 58, x + col_w - 6, y + 104);

    layout->metric_label[idx] = offset_rect_y(label, -scroll);
    layout->metric_input_frame[idx] = offset_rect_y(frame, -scroll);
    layout->metric_input[idx] = offset_rect_y(input, -scroll);
    layout->metric_help[idx] = offset_rect_y(help, -scroll);
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
    int metric_gap = 12;
    int metric_col_w = (width - metric_gap) / 2;
    int swatch_size = 24;
    int swatch_gap = 8;
    int bottom_margin = 18;
    int i;

    memset(layout, 0, sizeof(*layout));
    layout->viewport = make_rect(panel_x, TOP_BAR_H + 52, client.right - FORM_X_PAD, client.bottom - bottom_margin);
    layout->scroll_offset = scroll_offset;

    layout->title = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 28), -scroll_offset);
    y += 36;
    place_section(&layout->stage_section, panel_x, width, &y, scroll_offset);
    for (i = 0; i < 4; i++) {
        layout->stage_row[i] = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 19), -scroll_offset);
        y += 21;
    }
    y += 6;
    place_section(&layout->viewer_section, panel_x, width, &y, scroll_offset);
    for (i = 0; i < 3; i++) {
        layout->viewer_row[i] = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 20), -scroll_offset);
        y += 22;
    }
    y += 6;
    place_section(&layout->generation_section, panel_x, width, &y, scroll_offset);
    place_section(&layout->map_size_section, panel_x, width, &y, scroll_offset);
    for (i = 0; i < MAP_SIZE_COUNT; i++) {
        RECT button = make_rect(panel_x + i * (button_w + gap), y, panel_x + i * (button_w + gap) + button_w, y + 28);
        layout->map_size_buttons[i] = offset_rect_y(button, -scroll_offset);
    }
    y += 34;
    layout->map_size_help = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 18), -scroll_offset);
    y += 26;
    layout->initial_label = offset_rect_y(make_rect(panel_x, y, panel_x + width - 92, y + 28), -scroll_offset);
    layout->initial_input_frame = offset_rect_y(make_rect(panel_x + width - 84, y, panel_x + width, y + 30), -scroll_offset);
    layout->initial_input = offset_rect_y(inset_rect(make_rect(panel_x + width - 84, y, panel_x + width, y + 30), 5, 3), -scroll_offset);
    y += 34;
    layout->initial_help = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 18), -scroll_offset);
    y += 30;

    place_section(&layout->physical_section, panel_x, width, &y, scroll_offset);
    layout->physical_random_button = section_button(layout->physical_section);
    for (i = WORLD_SLIDER_OCEAN; i <= WORLD_SLIDER_VEGETATION; i++) {
        place_slider(&layout->sliders[i], panel_x, width, &y, scroll_offset);
    }
    y += 4;
    place_section(&layout->terrain_section, panel_x, width, &y, scroll_offset);
    layout->terrain_random_button = section_button(layout->terrain_section);
    for (i = WORLD_SLIDER_BIAS_FOREST; i <= WORLD_SLIDER_BIAS_WETLAND; i++) {
        place_slider(&layout->sliders[i], panel_x, width, &y, scroll_offset);
    }
    y += 4;
    place_section(&layout->regions_section, panel_x, width, &y, scroll_offset);
    place_slider(&layout->sliders[UI_SLIDER_REGION_SIZE], panel_x, width, &y, scroll_offset);
    layout->region_estimate = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 20), -scroll_offset);
    y += 22;
    layout->region_warning = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 34), -scroll_offset);
    y += 38;
    layout->generate_row = offset_rect_y(make_rect(panel_x, y + 2, panel_x + width, y + 24), -scroll_offset);
    y += 36;

    place_section(&layout->civ_section, panel_x, width, &y, scroll_offset);
    layout->civ_random_button = section_button(layout->civ_section);
    layout->civ_hint = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 20), -scroll_offset);
    y += 24;
    layout->civ_range_help = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 40), -scroll_offset);
    y += 44;
    layout->name_label = offset_rect_y(make_rect(panel_x, y, panel_x + 160, y + 18), -scroll_offset);
    layout->symbol_label = offset_rect_y(make_rect(panel_x + 176, y, panel_x + 238, y + 18), -scroll_offset);
    layout->name_input_frame = offset_rect_y(make_rect(panel_x, y + 22, panel_x + 164, y + 52), -scroll_offset);
    layout->symbol_input_frame = offset_rect_y(make_rect(panel_x + 176, y + 22, panel_x + 232, y + 52), -scroll_offset);
    layout->name_input = offset_rect_y(inset_rect(make_rect(panel_x, y + 22, panel_x + 164, y + 52), 5, 3), -scroll_offset);
    layout->symbol_input = offset_rect_y(inset_rect(make_rect(panel_x + 176, y + 22, panel_x + 232, y + 52), 5, 3), -scroll_offset);
    y += 62;
    layout->civ_color_label = offset_rect_y(make_rect(panel_x, y, panel_x + width, y + 18), -scroll_offset);
    y += 22;
    for (i = 0; i < CIV_COLOR_PALETTE_COUNT; i++) {
        int col = i % 8;
        int row = i / 8;
        RECT swatch = make_rect(panel_x + col * (swatch_size + swatch_gap),
                                y + row * (swatch_size + swatch_gap),
                                panel_x + col * (swatch_size + swatch_gap) + swatch_size,
                                y + row * (swatch_size + swatch_gap) + swatch_size);
        layout->civ_color_swatch[i] = offset_rect_y(swatch, -scroll_offset);
    }
    y += 2 * swatch_size + swatch_gap + 14;
    place_metric(layout, WORLDGEN_METRIC_MILITARY, panel_x, y, metric_col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_LOGISTICS, panel_x + metric_col_w + metric_gap, y, metric_col_w, scroll_offset);
    y += 112;
    place_metric(layout, WORLDGEN_METRIC_GOVERNANCE, panel_x, y, metric_col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_COHESION, panel_x + metric_col_w + metric_gap, y, metric_col_w, scroll_offset);
    y += 112;
    place_metric(layout, WORLDGEN_METRIC_PRODUCTION, panel_x, y, metric_col_w, scroll_offset);
    place_metric(layout, WORLDGEN_METRIC_COMMERCE, panel_x + metric_col_w + metric_gap, y, metric_col_w, scroll_offset);
    y += 112;
    place_metric(layout, WORLDGEN_METRIC_INNOVATION, panel_x, y, metric_col_w, scroll_offset);
    y += 112;
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
