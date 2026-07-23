#include "ui/ui_worldgen_input.h"

#include <string.h>

#include "ui/ui_forms.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_panel_layout.h"
#include "ui/ui_worldgen_view.h"

static UiWorldgenInputDiagnostics diagnostics;

static UiWorldgenControlTarget target(UiWorldgenControlKind kind, int index) {
    UiWorldgenControlTarget value = {kind, index};
    return value;
}

static UiWorldgenControlTarget no_target(void) {
    return target(UI_WORLDGEN_CONTROL_NONE, -1);
}

int ui_worldgen_input_owns_panel_point(RECT client, int panel_width,
                                       int mouse_x, int mouse_y) {
    UiWorldgenPanelLayout layout;
    ui_worldgen_view_build(client, panel_width, &layout);
    return mouse_x > layout.panel.left && mouse_x < layout.panel.right &&
           mouse_y >= layout.panel.top && mouse_y < layout.panel.bottom;
}

static int target_equal(UiWorldgenControlTarget left,
                        UiWorldgenControlTarget right) {
    return left.kind == right.kind && left.index == right.index;
}

static int hit(RECT viewport, RECT rect, int x, int y) {
    return ui_worldgen_panel_hit_test(viewport, rect, x, y);
}

static UiWorldgenControlTarget hit_common(const UiWorldgenPanelLayout *layout,
                                          int x, int y) {
    int i;
    if (x < layout->panel.left || x >= layout->panel.right) return no_target();
    if (x >= layout->preset_selector.left && x < layout->preset_selector.right &&
        y >= layout->preset_selector.top && y < layout->preset_selector.bottom)
        return target(UI_WORLDGEN_CONTROL_PRESET, 0);
    if (x >= layout->dice_button.left && x < layout->dice_button.right &&
        y >= layout->dice_button.top && y < layout->dice_button.bottom)
        return target(UI_WORLDGEN_CONTROL_DICE, 0);
    if (x >= layout->reset_button.left && x < layout->reset_button.right &&
        y >= layout->reset_button.top && y < layout->reset_button.bottom)
        return target(UI_WORLDGEN_CONTROL_RESET, 0);
    for (i = 0; i < UI_WORLDGEN_TAB_COUNT; i++) {
        if (x >= layout->tab_button[i].left && x < layout->tab_button[i].right &&
            y >= layout->tab_button[i].top && y < layout->tab_button[i].bottom)
            return target(UI_WORLDGEN_CONTROL_TAB, i);
    }
    if (x >= layout->generate_button.left && x < layout->generate_button.right &&
        y >= layout->generate_button.top && y < layout->generate_button.bottom)
        return target(UI_WORLDGEN_CONTROL_FOOTER_GENERATE, 0);
    return no_target();
}

static UiWorldgenControlTarget hit_physical(
    const UiWorldgenPanelLayout *layout, int x, int y) {
    int i;
    for (i = 0; i < UI_WORLDGEN_PANEL_MAP_SIZE_COUNT; i++) {
        if (hit(layout->content_viewport, layout->physical.map_size_button[i], x, y))
            return target(UI_WORLDGEN_CONTROL_MAP_SIZE, i);
    }
    for (i = 0; i < UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT; i++) {
        if (hit(layout->content_viewport, layout->physical.relief_handle_hit[i], x, y))
            return target(UI_WORLDGEN_CONTROL_RELIEF_HANDLE, i);
    }
    if (hit(layout->content_viewport, layout->physical.xy_handle_hit, x, y) ||
        hit(layout->content_viewport, layout->physical.xy_plot, x, y))
        return target(UI_WORLDGEN_CONTROL_PHYSICAL_XY, 0);
    return no_target();
}

static UiWorldgenControlTarget hit_climate(
    const UiWorldgenPanelLayout *layout, int x, int y) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    int matches[UI_WORLDGEN_CLIMATE_CORNER_COUNT] = {0};
    int first = -1;
    int count = 0;
    int i;
    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        if (hit(layout->content_viewport,
                layout->climate.corner_handle_hit[i], x, y)) {
            matches[i] = 1;
            if (first < 0) first = i;
            count++;
        }
    }
    if (count > 1 &&
        state->interaction.focused.kind == UI_WORLDGEN_CONTROL_CLIMATE_CORNER) {
        for (i = 1; i <= UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
            int candidate = (state->interaction.focused.index + i) %
                            UI_WORLDGEN_CLIMATE_CORNER_COUNT;
            if (matches[candidate])
                return target(UI_WORLDGEN_CONTROL_CLIMATE_CORNER, candidate);
        }
    }
    if (first >= 0) return target(UI_WORLDGEN_CONTROL_CLIMATE_CORNER, first);
    if (hit(layout->content_viewport, layout->climate.vegetation_handle_hit, x, y) ||
        hit(layout->content_viewport, layout->climate.vegetation_track, x, y))
        return target(UI_WORLDGEN_CONTROL_VEGETATION, 0);
    return no_target();
}

static UiWorldgenControlTarget hit_hydrology(
    const UiWorldgenPanelLayout *layout, int x, int y) {
    int i;
    if (hit(layout->content_viewport, layout->hydrology.river_handle_hit, x, y) ||
        hit(layout->content_viewport, layout->hydrology.river_track, x, y))
        return target(UI_WORLDGEN_CONTROL_RIVER_DENSITY,
                      UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT);
    for (i = 0; i < UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT; i++) {
        if (hit(layout->content_viewport, layout->hydrology.river_slot[i], x, y) ||
            hit(layout->content_viewport, layout->hydrology.river_slot_label[i], x, y))
            return target(UI_WORLDGEN_CONTROL_RIVER_DENSITY, i);
    }
    for (i = 0; i < UI_WORLDGEN_REGION_PRESET_COUNT; i++) {
        if (hit(layout->content_viewport, layout->hydrology.region_slot[i], x, y) ||
            hit(layout->content_viewport, layout->hydrology.region_slot_label[i], x, y))
            return target(UI_WORLDGEN_CONTROL_REGION_CATEGORY, i);
    }
    if (hit(layout->content_viewport, layout->hydrology.region_custom_label, x, y))
        return target(UI_WORLDGEN_CONTROL_REGION_CATEGORY,
                      UI_WORLDGEN_REGION_CUSTOM);
    return no_target();
}

static UiWorldgenControlTarget hit_target(const UiWorldgenPanelLayout *layout,
                                          int x, int y) {
    UiWorldgenControlTarget value = hit_common(layout, x, y);
    if (value.kind != UI_WORLDGEN_CONTROL_NONE) return value;
    if (!ui_worldgen_panel_point_in_viewport(layout->content_viewport, x, y))
        return no_target();
    if (layout->tab == UI_WORLDGEN_TAB_PHYSICAL) return hit_physical(layout, x, y);
    if (layout->tab == UI_WORLDGEN_TAB_CLIMATE) return hit_climate(layout, x, y);
    if (layout->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS)
        return hit_hydrology(layout, x, y);
    return no_target();
}

static int draggable(UiWorldgenControlTarget value) {
    return value.kind == UI_WORLDGEN_CONTROL_PHYSICAL_XY ||
           value.kind == UI_WORLDGEN_CONTROL_RELIEF_HANDLE ||
           value.kind == UI_WORLDGEN_CONTROL_CLIMATE_CORNER ||
           value.kind == UI_WORLDGEN_CONTROL_VEGETATION ||
           (value.kind == UI_WORLDGEN_CONTROL_RIVER_DENSITY &&
            value.index == UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT);
}

static int update_drag(UiWorldgenControlTarget value,
                       const UiWorldgenPanelLayout *layout, int x, int y) {
    int first, second;
    int changed = 0;
    if (value.kind == UI_WORLDGEN_CONTROL_PHYSICAL_XY) {
        ui_worldgen_panel_point_to_values(layout->physical.xy_plot,
            (POINT){x, y}, 0, 100, 0, 100, &first, &second);
        changed |= ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_CONTINENT, first);
        changed |= ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_OCEAN, second);
    } else if (value.kind == UI_WORLDGEN_CONTROL_RELIEF_HANDLE) {
        first = ui_worldgen_panel_axis_to_value(x,
            layout->physical.relief_baseline.left,
            layout->physical.relief_baseline.right - 1, 0, 100);
        changed = ui_worldgen_control_state_set_field(
            value.index == 0 ? UI_WORLDGEN_FIELD_RELIEF :
                               UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, first);
    } else if (value.kind == UI_WORLDGEN_CONTROL_CLIMATE_CORNER) {
        ui_worldgen_panel_point_to_values(layout->climate.plot,
            (POINT){x, y}, -50, 50, -50, 50, &first, &second);
        changed = ui_worldgen_control_state_set_climate_corner(
            (UiWorldgenClimateCornerId)value.index, first, second);
    } else if (value.kind == UI_WORLDGEN_CONTROL_VEGETATION) {
        first = ui_worldgen_panel_axis_to_value(x,
            layout->climate.vegetation_track.left,
            layout->climate.vegetation_track.right - 1, 0, 100);
        changed = ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_VEGETATION, first);
    } else if (value.kind == UI_WORLDGEN_CONTROL_RIVER_DENSITY) {
        first = ui_worldgen_panel_axis_to_value(x,
            layout->hydrology.river_track.left,
            layout->hydrology.river_track.right - 1, 0, 100);
        changed = ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_BIAS_WETLAND, first);
    }
    if (changed) {
        diagnostics.value_writes++;
        diagnostics.drag_updates++;
    }
    return changed;
}

static int activate(HWND hwnd, UiWorldgenControlTarget value) {
    int changed = 0;
    if (value.kind == UI_WORLDGEN_CONTROL_PRESET) {
        changed = ui_worldgen_control_state_apply_balanced();
    } else if (value.kind == UI_WORLDGEN_CONTROL_DICE) {
        return ui_worldgen_command_dice(hwnd);
    } else if (value.kind == UI_WORLDGEN_CONTROL_RESET) {
        return ui_worldgen_command_reset(hwnd);
    } else if (value.kind == UI_WORLDGEN_CONTROL_TAB) {
        changed = ui_worldgen_control_state_set_tab(
            (UiWorldgenControlTab)value.index);
    } else if (value.kind == UI_WORLDGEN_CONTROL_MAP_SIZE) {
        changed = ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, value.index);
    } else if (value.kind == UI_WORLDGEN_CONTROL_RIVER_DENSITY) {
        changed = ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_BIAS_WETLAND, value.index * 25);
    } else if (value.kind == UI_WORLDGEN_CONTROL_REGION_CATEGORY) {
        changed = ui_worldgen_control_state_set_region_category(
            (UiWorldgenRegionCategory)value.index);
    } else if (value.kind == UI_WORLDGEN_CONTROL_FOOTER_GENERATE) {
        return ui_worldgen_command_generate(hwnd) || 1;
    }
    if (changed) diagnostics.value_writes++;
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_worldgen_input_mouse_down(HWND hwnd, RECT client, int panel_width,
                                 int mouse_x, int mouse_y) {
    UiWorldgenPanelLayout layout;
    UiWorldgenControlTarget value;
    diagnostics.mouse_downs++;
    ui_worldgen_view_build(client, panel_width, &layout);
    value = hit_target(&layout, mouse_x, mouse_y);
    if (value.kind == UI_WORLDGEN_CONTROL_NONE) return 0;
    if (ui_forms_commit_focused_worldgen_numeric()) SetFocus(hwnd);
    ui_worldgen_control_state_set_focused(value);
    if (draggable(value)) {
        ui_worldgen_control_state_begin_drag(value);
        update_drag(value, &layout, mouse_x, mouse_y);
    } else {
        ui_worldgen_control_state_set_pressed(value);
    }
    SetCapture(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_worldgen_input_mouse_move(HWND hwnd, RECT client, int panel_width,
                                 int mouse_x, int mouse_y) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenControlTarget dragging = state->interaction.dragging;
    UiWorldgenPanelLayout layout;
    diagnostics.mouse_moves++;
    ui_worldgen_view_build(client, panel_width, &layout);
    if (dragging.kind != UI_WORLDGEN_CONTROL_NONE) {
        if (update_drag(dragging, &layout, mouse_x, mouse_y))
            ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (ui_worldgen_control_state_set_hovered(
            hit_target(&layout, mouse_x, mouse_y))) {
        ui_invalidate_side_panel_hover(hwnd);
    }
    return 0;
}

int ui_worldgen_input_mouse_up(HWND hwnd, RECT client, int panel_width,
                               int mouse_x, int mouse_y) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenControlTarget pressed = state->interaction.pressed;
    UiWorldgenControlTarget dragging = state->interaction.dragging;
    UiWorldgenPanelLayout layout;
    UiWorldgenControlTarget released;
    diagnostics.mouse_ups++;
    if (pressed.kind == UI_WORLDGEN_CONTROL_NONE &&
        dragging.kind == UI_WORLDGEN_CONTROL_NONE) return 0;
    ui_worldgen_view_build(client, panel_width, &layout);
    released = hit_target(&layout, mouse_x, mouse_y);
    if (dragging.kind != UI_WORLDGEN_CONTROL_NONE) {
        update_drag(dragging, &layout, mouse_x, mouse_y);
        ui_worldgen_control_state_end_drag();
        ui_forms_write_world_setup_controls();
        ui_forms_layout(hwnd);
        ReleaseCapture();
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    ui_worldgen_control_state_end_drag();
    ReleaseCapture();
    if (target_equal(pressed, released)) return activate(hwnd, pressed);
    diagnostics.release_outside_cancels++;
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_worldgen_input_mouse_leave(HWND hwnd) {
    UiWorldgenControlTarget none = no_target();
    if (ui_worldgen_control_state_set_hovered(none)) {
        ui_invalidate_side_panel_hover(hwnd);
        return 1;
    }
    return 0;
}

int ui_worldgen_input_wheel(HWND hwnd, RECT client, int panel_width,
                            int mouse_x, int mouse_y, int steps) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenPanelLayout layout;
    int next;
    if (steps == 0) return 0;
    if (ui_forms_commit_focused_worldgen_numeric()) SetFocus(hwnd);
    ui_worldgen_view_build(client, panel_width, &layout);
    if (!ui_worldgen_panel_point_in_viewport(layout.content_viewport,
                                             mouse_x, mouse_y)) return 0;
    next = layout.scroll_offset - steps * 72;
    if (next < 0) next = 0;
    if (next > layout.max_scroll) next = layout.max_scroll;
    if (!ui_worldgen_control_state_set_scroll(state->tab, next)) return 1;
    diagnostics.wheel_changes++;
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_worldgen_input_key_down(HWND hwnd, WPARAM key) {
    if (key != VK_RETURN ||
        !ui_forms_commit_focused_worldgen_numeric()) return 0;
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

void ui_worldgen_input_get_diagnostics(UiWorldgenInputDiagnostics *out) {
    if (out) *out = diagnostics;
}

void ui_worldgen_input_reset_diagnostics(void) {
    memset(&diagnostics, 0, sizeof(diagnostics));
}
