#include "ui/ui_worldgen_panel_layout.h"
#include "core/constants.h"
#include <string.h>
enum {
    PANEL_PAD = 12, SHELL_GAP = 8, FINGERPRINT_HEIGHT = 158,
    TAB_HEIGHT = 42, FOOTER_HEIGHT = 58, HANDLE_SIZE = 14,
    HANDLE_HIT_SIZE = 22
};
static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}
static int min_int(int left, int right) { return left < right ? left : right; }
static int max_int(int left, int right) { return left > right ? left : right; }
static RECT make_rect(int left, int top, int right, int bottom) {
    RECT rect = {left, top, right, bottom};
    return rect;
}
static RECT inset_rect(RECT rect, int x, int y) {
    rect.left += x; rect.right -= x;
    rect.top += y; rect.bottom -= y;
    return rect;
}
static RECT offset_rect_y(RECT rect, int offset) {
    rect.top += offset; rect.bottom += offset;
    return rect;
}
static RECT centered_rect(POINT center, int size) {
    int before = size / 2;
    return make_rect(center.x - before, center.y - before,
                     center.x - before + size, center.y - before + size);
}
static long long divide_round_nearest(long long numerator, long long denominator) {
    if (denominator < 0) {
        numerator = -numerator; denominator = -denominator;
    }
    if (denominator == 0) return 0;
    if (numerator < 0) return -((-numerator + denominator / 2) / denominator);
    return (numerator + denominator / 2) / denominator;
}
RECT ui_worldgen_panel_aspect_fit(RECT bounds, int source_width,
                                  int source_height) {
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    int fitted_width;
    int fitted_height;
    if (width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0) {
        return make_rect(bounds.left, bounds.top, bounds.left, bounds.top);
    }
    fitted_width = width;
    fitted_height = (int)((long long)width * source_height / source_width);
    if (fitted_height > height) {
        fitted_height = height;
        fitted_width = (int)((long long)height * source_width / source_height);
    }
    fitted_width = max_int(1, fitted_width); fitted_height = max_int(1, fitted_height);
    return make_rect(bounds.left + (width - fitted_width) / 2,
                     bounds.top + (height - fitted_height) / 2,
                     bounds.left + (width - fitted_width) / 2 + fitted_width,
                     bounds.top + (height - fitted_height) / 2 + fitted_height);
}
RECT ui_worldgen_panel_clip_rect(RECT viewport, RECT rect) {
    RECT clipped = {max_int(viewport.left, rect.left), max_int(viewport.top, rect.top),
                    min_int(viewport.right, rect.right), min_int(viewport.bottom, rect.bottom)};
    if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
        return make_rect(0, 0, 0, 0);
    return clipped;
}
int ui_worldgen_panel_point_in_viewport(RECT viewport, int x, int y) {
    return x >= viewport.left && x < viewport.right &&
           y >= viewport.top && y < viewport.bottom;
}
int ui_worldgen_panel_hit_test(RECT viewport, RECT rect, int x, int y) {
    RECT clipped = ui_worldgen_panel_clip_rect(viewport, rect);
    return clipped.right > clipped.left && clipped.bottom > clipped.top &&
           x >= clipped.left && x < clipped.right &&
           y >= clipped.top && y < clipped.bottom;
}
int ui_worldgen_panel_value_to_axis(int value, int value_min, int value_max,
                                    int axis_start, int axis_end) {
    long long numerator;
    if (value_max <= value_min) return axis_start;
    value = clamp_int(value, value_min, value_max);
    numerator = (long long)(value - value_min) * (axis_end - axis_start);
    return axis_start + (int)divide_round_nearest(numerator, value_max - value_min);
}
int ui_worldgen_panel_axis_to_value(int position, int axis_start, int axis_end,
                                    int value_min, int value_max) {
    long long numerator;
    int low = min_int(axis_start, axis_end);
    int high = max_int(axis_start, axis_end);
    if (axis_start == axis_end || value_max <= value_min) return value_min;
    position = clamp_int(position, low, high);
    numerator = (long long)(position - axis_start) * (value_max - value_min);
    return clamp_int(value_min + (int)divide_round_nearest(
                         numerator, axis_end - axis_start), value_min, value_max);
}
POINT ui_worldgen_panel_values_to_point(RECT plot, int x_value, int x_min,
                                        int x_max, int y_value, int y_min,
                                        int y_max) {
    POINT point;
    point.x = ui_worldgen_panel_value_to_axis(x_value, x_min, x_max,
                                              plot.left, max_int(plot.left, plot.right - 1));
    point.y = ui_worldgen_panel_value_to_axis(y_value, y_min, y_max,
                                              max_int(plot.top, plot.bottom - 1), plot.top);
    return point;
}
void ui_worldgen_panel_point_to_values(RECT plot, POINT point, int x_min,
                                       int x_max, int y_min, int y_max,
                                       int *out_x, int *out_y) {
    if (out_x) *out_x = ui_worldgen_panel_axis_to_value(
        point.x, plot.left, max_int(plot.left, plot.right - 1), x_min, x_max);
    if (out_y) *out_y = ui_worldgen_panel_axis_to_value(
        point.y, max_int(plot.top, plot.bottom - 1), plot.top, y_min, y_max);
}
static RECT label_at(RECT plot, int x_percent, int y_percent, int width) {
    POINT center = {plot.left + (plot.right - plot.left) * x_percent / 100,
                    plot.top + (plot.bottom - plot.top) * y_percent / 100};
    int half_limit = min_int(center.x - plot.left, plot.right - center.x);
    width = min_int(width, max_int(2, half_limit * 2));
    return make_rect(center.x - width / 2, center.y - 9,
                     center.x - width / 2 + width, center.y + 9);
}
static void split_slots(RECT bounds, RECT *slots, int count) {
    int i;
    int width = bounds.right - bounds.left;
    for (i = 0; i < count; i++) {
        slots[i] = make_rect(bounds.left + width * i / count, bounds.top,
                             bounds.left + width * (i + 1) / count, bounds.bottom);
    }
}
static UiWorldgenPoint climate_corner(const UiWorldgenPanelLayoutInput *input,
                                      int index) {
    static const UiWorldgenPoint defaults[UI_WORLDGEN_CLIMATE_CORNER_COUNT] = {
        {-25, 25}, {25, 25}, {25, -25}, {-25, -25}
    };
    const UiWorldgenClimateCorners *corners = input->climate_corners;
    if (!corners) return defaults[index];
    if (index == UI_WORLDGEN_CLIMATE_TOP_LEFT) return corners->top_left;
    if (index == UI_WORLDGEN_CLIMATE_TOP_RIGHT) return corners->top_right;
    if (index == UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT) return corners->bottom_right;
    return corners->bottom_left;
}
static int config_value(const UiWorldgenPanelLayoutInput *input,
                        UiWorldgenConfigField field) {
    const UiWorldgenEffectiveConfig *config = input->config;
    if (!config) return 50;
    if (field == UI_WORLDGEN_FIELD_OCEAN) return config->ocean_slider;
    if (field == UI_WORLDGEN_FIELD_CONTINENT) return config->continent_slider;
    if (field == UI_WORLDGEN_FIELD_RELIEF) return config->relief_slider;
    if (field == UI_WORLDGEN_FIELD_VEGETATION) return config->vegetation_slider;
    if (field == UI_WORLDGEN_FIELD_BIAS_MOUNTAIN) return config->bias_mountain_slider;
    if (field == UI_WORLDGEN_FIELD_BIAS_WETLAND) return config->bias_wetland_slider;
    return 50;
}
static void build_fingerprint(const UiWorldgenPanelLayoutInput *input,
                              UiWorldgenFingerprintLayout *layout, RECT card) {
    static const int vectors[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT][2] = {
        {0, -1024}, {800, -638}, {998, 228}, {445, 922},
        {-445, 922}, {-998, 228}, {-800, -638}
    };
    int i, diameter, legend_top;
    layout->card = card;
    layout->title = make_rect(card.left + 8, card.top + 4,
                              card.right - 8, card.top + 23);
    layout->legend_default = make_rect(card.right - 84, 0,
                                       card.right - 8, 0);
    layout->plot = make_rect(card.left + 8, card.top + 26,
                             layout->legend_default.left - 12, card.bottom - 8);
    diameter = min_int(124, min_int(layout->plot.right - layout->plot.left,
                                    layout->plot.bottom - layout->plot.top));
    layout->center.x = (layout->plot.left + layout->plot.right) / 2;
    layout->center.y = (layout->plot.top + layout->plot.bottom) / 2;
    layout->radius = diameter / 2 - 6;
    legend_top = card.top + (card.bottom - card.top - 46) / 2;
    layout->legend_default.top = legend_top;
    layout->legend_default.bottom = legend_top + 20;
    layout->legend_current = make_rect(card.right - 84, legend_top + 26,
                                       card.right - 8, legend_top + 46);
    for (i = 0; i < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; i++) {
        int value = input->fingerprint ? input->fingerprint->values[i] : 50;
        POINT end = {layout->center.x + vectors[i][0] * layout->radius / 1024,
                     layout->center.y + vectors[i][1] * layout->radius / 1024};
        layout->axis_end[i] = end;
        layout->default_point[i].x = layout->center.x +
            (end.x - layout->center.x) / 2;
        layout->default_point[i].y = layout->center.y +
            (end.y - layout->center.y) / 2;
        value = clamp_int(value, 0, 100);
        layout->current_point[i].x = layout->center.x +
            (end.x - layout->center.x) * value / 100;
        layout->current_point[i].y = layout->center.y +
            (end.y - layout->center.y) * value / 100;
        layout->axis_label[i] = ui_worldgen_panel_clip_rect(
            card, make_rect(end.x - 28, end.y - 9, end.x + 28, end.y + 9));
    }
}
static void build_shell(const UiWorldgenPanelLayoutInput *input,
                        UiWorldgenPanelLayout *layout) {
    RECT client = input->client;
    int client_width = max_int(1, client.right - client.left);
    int panel_width = clamp_int(input->panel_width, 1, client_width);
    int top = max_int(client.top, TOP_BAR_H + 52);
    int selector_right, i;
    layout->panel = make_rect(client.right - panel_width, client.top,
                              client.right, client.bottom);
    layout->inner = make_rect(layout->panel.left + PANEL_PAD, top,
                              layout->panel.right - PANEL_PAD, client.bottom - 8);
    layout->title = make_rect(layout->inner.left, top, layout->inner.right, top + 26);
    layout->subtitle = make_rect(layout->inner.left, top + 27,
                                 layout->inner.right, top + 47);
    layout->preset_row = make_rect(layout->inner.left, top + 55,
                                   layout->inner.right, top + 87);
    selector_right = layout->preset_row.right - 76;
    layout->preset_label = make_rect(layout->preset_row.left, layout->preset_row.top,
                                     layout->preset_row.left + 52, layout->preset_row.bottom);
    layout->preset_selector = make_rect(layout->preset_label.right + 4,
                                        layout->preset_row.top, selector_right,
                                        layout->preset_row.bottom);
    layout->dice_button = make_rect(selector_right + 6, layout->preset_row.top,
                                    selector_right + 38, layout->preset_row.bottom);
    layout->reset_button = make_rect(selector_right + 44, layout->preset_row.top,
                                     selector_right + 76, layout->preset_row.bottom);
    layout->header = make_rect(layout->inner.left, layout->title.top,
                               layout->inner.right, layout->preset_row.bottom);
    layout->tabs_row = make_rect(layout->inner.left, layout->preset_row.bottom + SHELL_GAP,
                                 layout->inner.right,
                                 layout->preset_row.bottom + SHELL_GAP + TAB_HEIGHT);
    for (i = 0; i < UI_WORLDGEN_TAB_COUNT; i++) {
        int left = layout->tabs_row.left +
                   (layout->tabs_row.right - layout->tabs_row.left) * i / UI_WORLDGEN_TAB_COUNT;
        int right = layout->tabs_row.left +
                    (layout->tabs_row.right - layout->tabs_row.left) * (i + 1) /
                    UI_WORLDGEN_TAB_COUNT;
        layout->tab_button[i] = make_rect(left, layout->tabs_row.top, right,
                                          layout->tabs_row.bottom);
        layout->tab_label[i] = inset_rect(layout->tab_button[i], 2, 3);
    }
    layout->footer = make_rect(layout->inner.left,
                               max_int(layout->tabs_row.bottom + 16,
                                       layout->inner.bottom - FOOTER_HEIGHT),
                               layout->inner.right, layout->inner.bottom);
    layout->content_viewport = make_rect(layout->inner.left,
                                         layout->tabs_row.bottom + SHELL_GAP,
                                         layout->inner.right,
                                         max_int(layout->tabs_row.bottom + SHELL_GAP,
                                                 layout->footer.top - SHELL_GAP));
    layout->generate_button = make_rect(layout->footer.left, layout->footer.top + 8,
                                        layout->footer.left +
                                            (layout->footer.right - layout->footer.left) * 43 / 100,
                                        layout->footer.bottom - 8);
    layout->status = make_rect(layout->generate_button.right + 8, layout->footer.top + 5,
                               layout->footer.right, layout->footer.bottom - 5);
}
static int build_physical(const UiWorldgenPanelLayoutInput *input, RECT viewport,
                          int scroll, UiWorldgenPhysicalLayout *layout) {
    int x = viewport.left;
    int width = viewport.right - viewport.left;
    int y = viewport.top + FINGERPRINT_HEIGHT + SHELL_GAP;
    int i;
    RECT raw;
    POINT point;
    layout->map_size_section = offset_rect_y(make_rect(x, y, x + width, y + 62), -scroll);
    layout->map_size_label = offset_rect_y(make_rect(x + 6, y + 4, x + width - 6, y + 24), -scroll);
    for (i = 0; i < UI_WORLDGEN_PANEL_MAP_SIZE_COUNT; i++) {
        layout->map_size_button[i] = offset_rect_y(make_rect(
            x + width * i / UI_WORLDGEN_PANEL_MAP_SIZE_COUNT, y + 28,
            x + width * (i + 1) / UI_WORLDGEN_PANEL_MAP_SIZE_COUNT, y + 60), -scroll);
    }
    y += 74;
    raw = ui_worldgen_panel_aspect_fit(make_rect(x + 30, y + 28, x + width - 12,
                                                  y + 28 + min_int(300, width - 42)),
                                       1254, 1254);
    layout->xy_section = offset_rect_y(make_rect(x, y, x + width, raw.bottom + 32), -scroll);
    layout->xy_title = offset_rect_y(make_rect(x + 6, y + 4, x + width - 6, y + 24), -scroll);
    layout->xy_asset = offset_rect_y(raw, -scroll);
    layout->xy_plot = inset_rect(layout->xy_asset, 8, 8);
    layout->xy_x_axis = make_rect(layout->xy_plot.left,
                                  (layout->xy_plot.top + layout->xy_plot.bottom) / 2,
                                  layout->xy_plot.right,
                                  (layout->xy_plot.top + layout->xy_plot.bottom) / 2 + 1);
    layout->xy_y_axis = make_rect((layout->xy_plot.left + layout->xy_plot.right) / 2,
                                  layout->xy_plot.top,
                                  (layout->xy_plot.left + layout->xy_plot.right) / 2 + 1,
                                  layout->xy_plot.bottom);
    layout->xy_x_low_label = make_rect(layout->xy_plot.left, layout->xy_asset.bottom + 2,
                                       layout->xy_plot.left + 112, layout->xy_asset.bottom + 22);
    layout->xy_x_high_label = make_rect(layout->xy_plot.right - 112,
                                        layout->xy_asset.bottom + 2,
                                        layout->xy_plot.right, layout->xy_asset.bottom + 22);
    layout->xy_y_high_label = make_rect(x, layout->xy_plot.top, layout->xy_asset.left - 4,
                                        layout->xy_plot.top + 36);
    layout->xy_y_low_label = make_rect(x, layout->xy_plot.bottom - 36,
                                       layout->xy_asset.left - 4, layout->xy_plot.bottom);
    point = ui_worldgen_panel_values_to_point(
        layout->xy_plot, config_value(input, UI_WORLDGEN_FIELD_CONTINENT), 0, 100,
        config_value(input, UI_WORLDGEN_FIELD_OCEAN), 0, 100);
    layout->xy_handle = centered_rect(point, HANDLE_SIZE);
    layout->xy_handle_hit = centered_rect(point, HANDLE_HIT_SIZE);
    y = raw.bottom + 44;
    raw = ui_worldgen_panel_aspect_fit(make_rect(x + 8, y + 50, x + width - 8,
                                                  y + 220), 2048, 768);
    layout->relief_section = offset_rect_y(make_rect(x, y, x + width, raw.bottom + 24), -scroll);
    layout->relief_title = offset_rect_y(make_rect(x + 6, y + 4, x + width - 6, y + 24), -scroll);
    layout->relief_label[0] = offset_rect_y(make_rect(x + 8, y + 27,
                                                       x + width / 2 - 4, y + 47), -scroll);
    layout->relief_label[1] = offset_rect_y(make_rect(x + width / 2 + 4, y + 27,
                                                       x + width - 8, y + 47), -scroll);
    layout->relief_asset = offset_rect_y(raw, -scroll);
    layout->relief_baseline = make_rect(layout->relief_asset.left + 12,
                                        layout->relief_asset.bottom - 16,
                                        layout->relief_asset.right - 12,
                                        layout->relief_asset.bottom - 13);
    for (i = 0; i < UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT; i++) {
        int value = config_value(input, i == 0 ? UI_WORLDGEN_FIELD_RELIEF :
                                                  UI_WORLDGEN_FIELD_BIAS_MOUNTAIN);
        POINT center = {ui_worldgen_panel_value_to_axis(
                            value, 0, 100, layout->relief_baseline.left,
                            max_int(layout->relief_baseline.left,
                                    layout->relief_baseline.right - 1)),
                        layout->relief_baseline.top + (i == 0 ? -12 : 12)};
        layout->relief_handle[i] = centered_rect(center, HANDLE_SIZE);
        layout->relief_handle_hit[i] = centered_rect(center, HANDLE_HIT_SIZE);
    }
    y = raw.bottom + 36;
    return y - viewport.top;
}
static int build_climate(const UiWorldgenPanelLayoutInput *input, RECT viewport,
                         int scroll, UiWorldgenClimateLayout *layout) {
    static const int label_positions[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT][2] = {
        {24, 82}, {40, 10}, {47, 29}, {76, 82}, {12, 48}, {72, 60}, {83, 30}};
    static const int label_widths[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT] = {
        56, 56, 80, 56, 56, 60, 84};
    int x = viewport.left;
    int width = viewport.right - viewport.left;
    int y = viewport.top + FINGERPRINT_HEIGHT + SHELL_GAP;
    int gutter = clamp_int(width / 10, 36, 48), i;
    int plot_width = width - gutter * 2;
    RECT raw = make_rect(x + gutter, y + 50, x + width - gutter,
                         y + 50 + (plot_width * 2 + 1) / 3);
    layout->section = offset_rect_y(make_rect(x, y, x + width, raw.bottom + 30), -scroll);
    layout->title = offset_rect_y(make_rect(x + 6, y + 4, x + width - 6, y + 24), -scroll);
    layout->asset = offset_rect_y(raw, -scroll);
    layout->plot = layout->asset;
    layout->x_axis = make_rect(layout->plot.left,
                               (layout->plot.top + layout->plot.bottom) / 2,
                               layout->plot.right,
                               (layout->plot.top + layout->plot.bottom) / 2 + 1);
    layout->y_axis = make_rect((layout->plot.left + layout->plot.right) / 2,
                               layout->plot.top,
                               (layout->plot.left + layout->plot.right) / 2 + 1,
                               layout->plot.bottom);
    layout->y_high_label = make_rect(layout->plot.left, layout->asset.top - 23,
                                     layout->plot.right, layout->asset.top - 3);
    layout->y_low_label = make_rect(layout->plot.left, layout->asset.bottom + 3,
                                    layout->plot.right, layout->asset.bottom + 25);
    layout->x_low_label = make_rect(x, (layout->plot.top + layout->plot.bottom) / 2 - 18 + (gutter == 36 ? 28 : 0),
                                    layout->plot.left, (layout->plot.top + layout->plot.bottom) / 2 + 18 + (gutter == 36 ? 28 : 0));
    layout->x_high_label = make_rect(layout->plot.right, (layout->plot.top + layout->plot.bottom) / 2 - 18 + (gutter == 36 ? 28 : 0),
                                     x + width, (layout->plot.top + layout->plot.bottom) / 2 + 18 + (gutter == 36 ? 28 : 0));
    for (i = 0; i < UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT; i++)
        layout->biome_label[i] = label_at(layout->plot, label_positions[i][0],
                                          label_positions[i][1],
                                          max_int(label_widths[i], (layout->plot.right - layout->plot.left) / 5));
    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        UiWorldgenPoint value = climate_corner(input, i);
        POINT center = ui_worldgen_panel_values_to_point(layout->plot, value.x, -50, 50,
                                                          value.y, -50, 50);
        layout->corner_handle[i] = centered_rect(center, HANDLE_SIZE);
        layout->corner_handle_hit[i] = centered_rect(center, HANDLE_HIT_SIZE);
    }
    y = raw.bottom + 42;
    layout->vegetation_section = offset_rect_y(make_rect(x, y, x + width, y + 74), -scroll);
    layout->vegetation_label = offset_rect_y(make_rect(x + 6, y + 5,
                                                        x + width - 72, y + 25), -scroll);
    layout->vegetation_value = offset_rect_y(make_rect(x + width - 68, y + 5,
                                                        x + width - 6, y + 25), -scroll);
    layout->vegetation_track = offset_rect_y(make_rect(x + 12, y + 40,
                                                        x + width - 12, y + 46), -scroll);
    {
        POINT center = {ui_worldgen_panel_value_to_axis(
                            config_value(input, UI_WORLDGEN_FIELD_VEGETATION), 0, 100,
                            layout->vegetation_track.left,
                            max_int(layout->vegetation_track.left,
                                    layout->vegetation_track.right - 1)),
                        (layout->vegetation_track.top + layout->vegetation_track.bottom) / 2};
        layout->vegetation_handle = centered_rect(center, HANDLE_SIZE);
        layout->vegetation_handle_hit = centered_rect(center, HANDLE_HIT_SIZE);
    }
    return y + 86 - viewport.top;
}
static int build_hydrology(const UiWorldgenPanelLayoutInput *input, RECT viewport,
                           int scroll, UiWorldgenHydrologyLayout *layout) {
    int x = viewport.left;
    int width = viewport.right - viewport.left;
    int y = viewport.top + FINGERPRINT_HEIGHT + SHELL_GAP;
    int i;
    RECT raw = ui_worldgen_panel_aspect_fit(
        make_rect(x + 8, y + 30, x + width - 8, y + 175), 2172, 724);
    layout->river_title = offset_rect_y(make_rect(x + 6, y + 4,
                                                   x + width - 6, y + 24), -scroll);
    layout->river_asset = offset_rect_y(raw, -scroll);
    split_slots(layout->river_asset, layout->river_slot, UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT);
    for (i = 0; i < UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT; i++) {
        layout->river_slot_label[i] = make_rect(layout->river_slot[i].left,
                                                layout->river_asset.bottom + 2,
                                                layout->river_slot[i].right,
                                                layout->river_asset.bottom + 38);
    }
    layout->river_track = make_rect(layout->river_asset.left + 8,
                                    layout->river_asset.bottom + 50,
                                    layout->river_asset.right - 8,
                                    layout->river_asset.bottom + 56);
    {
        POINT center = {ui_worldgen_panel_value_to_axis(
                            config_value(input, UI_WORLDGEN_FIELD_BIAS_WETLAND), 0, 100,
                            layout->river_track.left,
                            max_int(layout->river_track.left, layout->river_track.right - 1)),
                        (layout->river_track.top + layout->river_track.bottom) / 2};
        layout->river_handle = centered_rect(center, HANDLE_SIZE);
        layout->river_handle_hit = centered_rect(center, HANDLE_HIT_SIZE);
    }
    layout->river_section = offset_rect_y(make_rect(x, y, x + width,
                                                     raw.bottom + 76), -scroll);
    y = raw.bottom + 88;
    raw = ui_worldgen_panel_aspect_fit(
        make_rect(x + 8, y + 30, x + width - 8, y + 210), 2048, 768);
    layout->region_title = offset_rect_y(make_rect(x + 6, y + 4,
                                                    x + width - 6, y + 24), -scroll);
    layout->region_asset = offset_rect_y(raw, -scroll);
    split_slots(layout->region_asset, layout->region_slot, UI_WORLDGEN_REGION_PRESET_COUNT);
    for (i = 0; i < UI_WORLDGEN_REGION_PRESET_COUNT; i++) {
        layout->region_slot_label[i] = make_rect(layout->region_slot[i].left,
                                                 layout->region_asset.bottom + 2,
                                                 layout->region_slot[i].right,
                                                 layout->region_asset.bottom + 38);
    }
    layout->region_custom_label = make_rect(x + 8, layout->region_asset.bottom + 46,
                                            x + width - 104, layout->region_asset.bottom + 76);
    layout->region_custom_input = make_rect(x + width - 96, layout->region_asset.bottom + 44,
                                            x + width - 8, layout->region_asset.bottom + 76);
    layout->region_target_area = make_rect(x + 8, layout->region_asset.bottom + 84,
                                           x + width - 8, layout->region_asset.bottom + 106);
    layout->region_estimated_count = make_rect(x + 8, layout->region_asset.bottom + 108,
                                               x + width - 8, layout->region_asset.bottom + 136);
    layout->region_section = offset_rect_y(make_rect(x, y, x + width, raw.bottom + 146), -scroll);
    y = raw.bottom + 158;
    layout->initial_civs_section = offset_rect_y(make_rect(x, y, x + width, y + 54), -scroll);
    layout->initial_civs_label = offset_rect_y(make_rect(x + 8, y + 12, x + width - 92, y + 42), -scroll);
    layout->initial_civs_input_frame = offset_rect_y(make_rect(x + width - 84, y + 12, x + width, y + 42), -scroll); layout->initial_civs_input = inset_rect(layout->initial_civs_input_frame, 5, 3); return y + 66 - viewport.top;
}
static int build_legacy(RECT viewport, int scroll, int requested_height,
                        UiWorldgenLegacyLayout *layout) {
    int inset = FINGERPRINT_HEIGHT + SHELL_GAP, viewport_height = max_int(0, viewport.bottom - viewport.top);
    int height = max_int(viewport_height, requested_height + inset);
    layout->viewport = viewport;
    layout->content_origin.x = viewport.left; layout->content_origin.y = viewport.top + inset - scroll;
    layout->content_width = max_int(0, viewport.right - viewport.left);
    layout->content_bounds = make_rect(viewport.left, viewport.top - scroll,
                                       viewport.right, viewport.top - scroll + height);
    return height;
}
void ui_worldgen_panel_layout_build(const UiWorldgenPanelLayoutInput *input,
                                    UiWorldgenPanelLayout *layout) {
    int viewport_height, i;
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    if (!input) return;
    build_shell(input, layout);
    viewport_height = max_int(0, layout->content_viewport.bottom - layout->content_viewport.top);
    layout->tab_content_height[UI_WORLDGEN_TAB_PHYSICAL] = build_physical(input, layout->content_viewport, 0, &layout->physical);
    layout->tab_content_height[UI_WORLDGEN_TAB_CLIMATE] = build_climate(input, layout->content_viewport, 0, &layout->climate);
    layout->tab_content_height[UI_WORLDGEN_TAB_HYDROLOGY_REGIONS] = build_hydrology(input, layout->content_viewport, 0, &layout->hydrology);
    layout->tab_content_height[UI_WORLDGEN_TAB_LEGACY] = build_legacy(layout->content_viewport, 0, input->legacy_content_height, &layout->legacy);
    for (i = 0; i < UI_WORLDGEN_TAB_COUNT; i++) {
        layout->tab_max_scroll[i] = max_int(0, layout->tab_content_height[i] - viewport_height);
        layout->tab_scroll_offset[i] = clamp_int(input->scroll_offsets[i], 0,
                                                 layout->tab_max_scroll[i]);
    }
    memset(&layout->physical, 0, sizeof(layout->physical)); memset(&layout->climate, 0, sizeof(layout->climate));
    memset(&layout->hydrology, 0, sizeof(layout->hydrology)); memset(&layout->legacy, 0, sizeof(layout->legacy));
    build_physical(input, layout->content_viewport, layout->tab_scroll_offset[UI_WORLDGEN_TAB_PHYSICAL], &layout->physical);
    build_climate(input, layout->content_viewport, layout->tab_scroll_offset[UI_WORLDGEN_TAB_CLIMATE], &layout->climate);
    build_hydrology(input, layout->content_viewport, layout->tab_scroll_offset[UI_WORLDGEN_TAB_HYDROLOGY_REGIONS], &layout->hydrology);
    build_legacy(layout->content_viewport, layout->tab_scroll_offset[UI_WORLDGEN_TAB_LEGACY],
                 input->legacy_content_height, &layout->legacy);
    layout->tab = input->tab >= UI_WORLDGEN_TAB_PHYSICAL && input->tab < UI_WORLDGEN_TAB_COUNT ?
                  input->tab : UI_WORLDGEN_TAB_PHYSICAL;
    layout->scroll_offset = layout->tab_scroll_offset[layout->tab];
    layout->content_height = layout->tab_content_height[layout->tab];
    layout->max_scroll = layout->tab_max_scroll[layout->tab];
    build_fingerprint(input, &layout->fingerprint, make_rect(layout->content_viewport.left,
        layout->content_viewport.top - layout->scroll_offset, layout->content_viewport.right,
        layout->content_viewport.top - layout->scroll_offset + FINGERPRINT_HEIGHT));
    layout->content_bounds = make_rect(layout->content_viewport.left,
        layout->content_viewport.top - layout->scroll_offset,
        layout->content_viewport.right,
        layout->content_viewport.top - layout->scroll_offset + layout->content_height);
}
