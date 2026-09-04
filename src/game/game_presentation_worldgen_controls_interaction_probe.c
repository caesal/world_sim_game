#include "game/game_presentation_worldgen_controls_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/constants.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_input.h"
#include "ui/ui_worldgen_panel_layout.h"
#include "ui/ui_worldgen_view.h"

#include <string.h>

static int rect_equal(RECT left, RECT right) {
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

static int rect_nonempty(RECT rect) {
    return rect.right > rect.left && rect.bottom > rect.top;
}

static POINT rect_center(RECT rect) {
    POINT point = {(rect.left + rect.right) / 2,
                   (rect.top + rect.bottom) / 2};
    return point;
}

static void build_layout_direct(int panel_width, UiWorldgenControlTab tab,
                                int scroll, UiWorldgenPanelLayout *layout) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenClimateCorners corners;
    UiWorldgenFingerprint fingerprint;
    UiWorldgenPanelLayoutInput input;
    ui_worldgen_config_read(&config);
    ui_worldgen_climate_corners_from_config(&config, &corners);
    ui_worldgen_config_fingerprint(&config, &fingerprint);
    memset(&input, 0, sizeof(input));
    input.client = (RECT){0, 0, 1280, 800};
    input.panel_width = panel_width;
    input.tab = tab;
    input.scroll_offsets[tab] = scroll;
    input.legacy_content_height = 1100;
    input.config = &config;
    input.climate_corners = &corners;
    input.fingerprint = &fingerprint;
    ui_worldgen_panel_layout_build(&input, layout);
}

static int shell_equal(const UiWorldgenPanelLayout *left,
                       const UiWorldgenPanelLayout *right) {
    int tab;
    int ok = rect_equal(left->header, right->header) &&
             rect_equal(left->title, right->title) &&
             rect_equal(left->subtitle, right->subtitle) &&
             rect_equal(left->preset_row, right->preset_row) &&
             rect_equal(left->tabs_row, right->tabs_row) &&
             rect_equal(left->footer, right->footer) &&
             rect_equal(left->generate_button, right->generate_button) &&
             rect_equal(left->status, right->status);
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ok &= rect_equal(left->tab_button[tab], right->tab_button[tab]);
    }
    return ok;
}

static int case_layout_widths_fixed_shell(
    WorldgenControlsProbeReport *report) {
    static const int widths[] = {340, 460};
    int ok = 1;
    int fixed_ok = 1;
    int bounds_ok = 1;
    int width_index;
    int tab;
    ui_worldgen_control_state_init_fresh();
    for (width_index = 0;
         width_index < (int)(sizeof(widths) / sizeof(widths[0]));
         width_index++) {
        UiWorldgenPanelLayout baseline;
        build_layout_direct(widths[width_index], UI_WORLDGEN_TAB_PHYSICAL,
                            0, &baseline);
        bounds_ok &= baseline.panel.right - baseline.panel.left ==
                     widths[width_index];
        bounds_ok &= baseline.header.bottom <= baseline.tabs_row.top &&
                     baseline.tabs_row.bottom <= baseline.content_viewport.top &&
                     baseline.content_viewport.bottom <= baseline.footer.top;
        bounds_ok &= rect_nonempty(baseline.preset_selector) &&
                     rect_nonempty(baseline.dice_button) &&
                     rect_nonempty(baseline.reset_button) &&
                     rect_nonempty(baseline.generate_button);
        for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
            UiWorldgenPanelLayout scrolled;
            build_layout_direct(widths[width_index],
                                (UiWorldgenControlTab)tab, 100000,
                                &scrolled);
            fixed_ok &= shell_equal(&baseline, &scrolled);
            bounds_ok &= scrolled.tab == (UiWorldgenControlTab)tab;
            bounds_ok &= scrolled.content_height > 0 &&
                         scrolled.max_scroll >= 0 &&
                         scrolled.scroll_offset == scrolled.max_scroll;
            if (scrolled.max_scroll > 0) {
                bounds_ok &= scrolled.content_bounds.top <
                             scrolled.content_viewport.top;
            }
        }
    }
    ok = fixed_ok && bounds_ok;
    worldgen_controls_probe_record(
        report, "worldgen_controls_layout_340_460_fixed_shell", ok,
        "widths=2 tabs=%d fixed_header_footer=%d bounds_ok=%d",
        UI_WORLDGEN_TAB_COUNT, fixed_ok, bounds_ok);
    return ok;
}

static int case_checkpoint_geometry(WorldgenControlsProbeReport *report) {
    static const int widths[] = {340, 460};
    static const int biome_positions[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT][2] = {
        {24, 82}, {40, 10}, {47, 29}, {76, 82},
        {12, 48}, {72, 60}, {83, 30}
    };
    static const int biome_widths[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT] = {
        56, 56, 80, 56, 56, 60, 84
    };
    int tabs_ok = 1;
    int fingerprint_ok = 1;
    int hydrology_ok = 1;
    int climate_ok = 1;
    int width_index;
    for (width_index = 0;
         width_index < (int)(sizeof(widths) / sizeof(widths[0]));
         width_index++) {
        UiWorldgenPanelLayout layout;
        UiWorldgenPanelLayout climate_layout;
        const UiWorldgenFingerprintLayout *fingerprint;
        const UiWorldgenHydrologyLayout *hydrology;
        const UiWorldgenClimateLayout *climate;
        int tab_width;
        int diameter;
        int axis;
        int biome;
        int tab;
        build_layout_direct(
            widths[width_index], UI_WORLDGEN_TAB_HYDROLOGY_REGIONS,
            0, &layout);
        tab_width = (layout.tabs_row.right - layout.tabs_row.left) /
                    UI_WORLDGEN_TAB_COUNT;
        for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
            tabs_ok &= layout.tab_button[tab].right -
                           layout.tab_button[tab].left == tab_width;
            tabs_ok &= layout.tab_label[tab].left ==
                           layout.tab_button[tab].left + 2 &&
                       layout.tab_label[tab].right ==
                           layout.tab_button[tab].right - 2 &&
                       layout.tab_label[tab].top ==
                           layout.tab_button[tab].top + 3 &&
                       layout.tab_label[tab].bottom ==
                           layout.tab_button[tab].bottom - 3;
            if (tab > 0) {
                tabs_ok &= layout.tab_button[tab - 1].right ==
                           layout.tab_button[tab].left;
            }
        }
        fingerprint = &layout.fingerprint;
        diameter = fingerprint->radius * 2 + 12;
        fingerprint_ok &=
            fingerprint->card.bottom - fingerprint->card.top == 158 &&
            fingerprint->legend_default.right ==
                fingerprint->card.right - 8 &&
            fingerprint->legend_default.right -
                fingerprint->legend_default.left == 76 &&
            fingerprint->plot.left == fingerprint->card.left + 8 &&
            fingerprint->plot.right ==
                fingerprint->legend_default.left - 12 &&
            fingerprint->plot.top == fingerprint->card.top + 26 &&
            fingerprint->plot.bottom == fingerprint->card.bottom - 8 &&
            diameter == 124 && fingerprint->radius == 56 &&
            fingerprint->center.x ==
                (fingerprint->plot.left + fingerprint->plot.right) / 2 &&
            fingerprint->center.y ==
                (fingerprint->plot.top + fingerprint->plot.bottom) / 2 &&
            fingerprint->legend_current.top ==
                fingerprint->legend_default.bottom + 6 &&
            fingerprint->legend_default.top +
                fingerprint->legend_current.bottom ==
                fingerprint->card.top + fingerprint->card.bottom;
        for (axis = 0; axis < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; axis++) {
            fingerprint_ok &=
                fingerprint->axis_label[axis].left >=
                    fingerprint->card.left &&
                fingerprint->axis_label[axis].right <=
                    fingerprint->card.right &&
                fingerprint->axis_label[axis].top >=
                    fingerprint->card.top &&
                fingerprint->axis_label[axis].bottom <=
                    fingerprint->card.bottom;
        }
        hydrology = &layout.hydrology;
        hydrology_ok &=
            hydrology->initial_civs_section.top -
                hydrology->region_section.bottom == 12 &&
            hydrology->initial_civs_input_frame.right -
                hydrology->initial_civs_input_frame.left == 84 &&
            hydrology->initial_civs_input_frame.bottom -
                hydrology->initial_civs_input_frame.top == 30 &&
            hydrology->initial_civs_input.left ==
                hydrology->initial_civs_input_frame.left + 5 &&
            hydrology->initial_civs_input.right ==
                hydrology->initial_civs_input_frame.right - 5 &&
            hydrology->initial_civs_input.top ==
                hydrology->initial_civs_input_frame.top + 3 &&
            hydrology->initial_civs_input.bottom ==
                hydrology->initial_civs_input_frame.bottom - 3 &&
            layout.content_bounds.bottom ==
                hydrology->initial_civs_section.bottom + 12;
        build_layout_direct(widths[width_index], UI_WORLDGEN_TAB_CLIMATE,
                            0, &climate_layout);
        climate = &climate_layout.climate;
        {
            int content_width = climate_layout.content_viewport.right -
                                climate_layout.content_viewport.left;
            int gutter = content_width / 10;
            int plot_width = climate->plot.right - climate->plot.left;
            int plot_height = climate->plot.bottom - climate->plot.top;
            if (gutter < 36) gutter = 36;
            if (gutter > 48) gutter = 48;
            climate_ok &= rect_equal(climate->asset, climate->plot) &&
                climate->plot.left - climate_layout.content_viewport.left == gutter &&
                climate_layout.content_viewport.right - climate->plot.right == gutter &&
                plot_width * 2 >= plot_height * 3 - 1 &&
                plot_width * 2 <= plot_height * 3 + 1;
            climate_ok &= climate->title.bottom <= climate->y_high_label.top &&
                climate->y_high_label.bottom <= climate->plot.top &&
                climate->plot.bottom <= climate->y_low_label.top &&
                climate->y_low_label.bottom <= climate->section.bottom &&
                climate->section.bottom < climate->vegetation_section.top;
            climate_ok &= climate->x_low_label.right == climate->plot.left &&
                climate->x_high_label.left == climate->plot.right &&
                rect_center(climate->x_low_label).y == rect_center(climate->plot).y +
                    (gutter == 36 ? 28 : 0) &&
                rect_center(climate->x_high_label).y == rect_center(climate->plot).y +
                    (gutter == 36 ? 28 : 0) &&
                rect_center(climate->y_high_label).x == rect_center(climate->plot).x &&
                rect_center(climate->y_low_label).x == rect_center(climate->plot).x;
            for (biome = 0; biome < UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT; biome++) {
                POINT center = rect_center(climate->biome_label[biome]);
                int expected_width = biome_widths[biome] > plot_width / 5 ?
                    biome_widths[biome] : plot_width / 5;
                climate_ok &= center.x == climate->plot.left +
                    plot_width * biome_positions[biome][0] / 100 &&
                    center.y == climate->plot.top +
                    plot_height * biome_positions[biome][1] / 100 &&
                    climate->biome_label[biome].right -
                    climate->biome_label[biome].left == expected_width &&
                    climate->biome_label[biome].left >= climate->plot.left &&
                    climate->biome_label[biome].right <= climate->plot.right &&
                    climate->biome_label[biome].top >= climate->plot.top &&
                    climate->biome_label[biome].bottom <= climate->plot.bottom;
            }
        }
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_checkpoint_geometry",
        tabs_ok && fingerprint_ok && hydrology_ok && climate_ok,
        "widths=340,460 equal_tabs=%d fingerprint_exact=%d hydrology_row=%d climate_geometry=%d",
        tabs_ok, fingerprint_ok, hydrology_ok, climate_ok);
    return tabs_ok && fingerprint_ok && hydrology_ok && climate_ok;
}

static int case_axis_boundaries(WorldgenControlsProbeReport *report) {
    static const int values[] = {0, 1, 25, 49, 50, 51, 75, 99, 100};
    RECT plot = {31, 47, 354, 370};
    int ok = 1;
    int samples = 0;
    int x;
    int y;
    for (x = 0; x < (int)(sizeof(values) / sizeof(values[0])); x++) {
        for (y = 0; y < (int)(sizeof(values) / sizeof(values[0])); y++) {
            POINT mapped = ui_worldgen_panel_values_to_point(
                plot, values[x], 0, 100, values[y], 0, 100);
            int round_x = -1;
            int round_y = -1;
            ui_worldgen_panel_point_to_values(
                plot, mapped, 0, 100, 0, 100, &round_x, &round_y);
            ok &= round_x == values[x] && round_y == values[y];
            samples++;
        }
    }
    ok &= ui_worldgen_panel_axis_to_value(-999, 10, 210, 0, 100) == 0;
    ok &= ui_worldgen_panel_axis_to_value(999, 10, 210, 0, 100) == 100;
    ok &= ui_worldgen_panel_axis_to_value(-999, 210, 10, 0, 100) == 100;
    ok &= ui_worldgen_panel_axis_to_value(999, 210, 10, 0, 100) == 0;
    worldgen_controls_probe_record(
        report, "worldgen_controls_xy_axis_boundaries", ok,
        "round_trip_samples=%d outside_clamps=4 ok=%d", samples, ok);
    return ok;
}

static int case_panel_routing_boundary(
    WorldgenControlsProbeReport *report) {
    RECT client = {0, 0, 1280, 800};
    UiWorldgenPanelLayout layout;
    int interior_ok;
    int map_ok;
    int divider_ok;
    build_layout_direct(460, UI_WORLDGEN_TAB_PHYSICAL, 0, &layout);
    interior_ok = ui_worldgen_input_owns_panel_point(
        client, 460, layout.panel.left + 12, layout.panel.bottom - 12);
    map_ok = !ui_worldgen_input_owns_panel_point(
        client, 460, layout.panel.left - 20, layout.panel.bottom / 2);
    divider_ok = !ui_worldgen_input_owns_panel_point(
        client, 460, layout.panel.left, layout.panel.bottom / 2);
    worldgen_controls_probe_record(
        report, "worldgen_controls_panel_routing_boundary",
        interior_ok && map_ok && divider_ok,
        "blank_panel_owned=%d map_falls_through=%d divider_falls_through=%d",
        interior_ok, map_ok, divider_ok);
    return interior_ok && map_ok && divider_ok;
}

static int case_relief_layout_identity(WorldgenControlsProbeReport *report) {
    UiWorldgenPanelLayout crossed_a;
    UiWorldgenPanelLayout crossed_b;
    UiWorldgenPanelLayout coincident;
    POINT first_center;
    POINT second_center;
    int crossed_ok;
    int coincident_ok;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF, 20);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 80);
    build_layout_direct(460, UI_WORLDGEN_TAB_PHYSICAL, 100000, &crossed_a);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF, 80);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 20);
    build_layout_direct(460, UI_WORLDGEN_TAB_PHYSICAL, 100000, &crossed_b);
    crossed_ok = crossed_a.physical.relief_handle[0].left <
                     crossed_a.physical.relief_handle[1].left &&
                 crossed_b.physical.relief_handle[0].left >
                     crossed_b.physical.relief_handle[1].left;
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF, 50);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 50);
    build_layout_direct(460, UI_WORLDGEN_TAB_PHYSICAL, 100000, &coincident);
    first_center = rect_center(coincident.physical.relief_handle_hit[0]);
    second_center = rect_center(coincident.physical.relief_handle_hit[1]);
    coincident_ok = first_center.x == second_center.x &&
                    first_center.y != second_center.y &&
                    ui_worldgen_panel_hit_test(
                        coincident.content_viewport,
                        coincident.physical.relief_handle_hit[0],
                        first_center.x, first_center.y) &&
                    ui_worldgen_panel_hit_test(
                        coincident.content_viewport,
                        coincident.physical.relief_handle_hit[1],
                        second_center.x, second_center.y) &&
                    !ui_worldgen_panel_hit_test(
                        coincident.content_viewport,
                        coincident.physical.relief_handle_hit[1],
                        first_center.x, first_center.y) &&
                    !ui_worldgen_panel_hit_test(
                        coincident.content_viewport,
                        coincident.physical.relief_handle_hit[0],
                        second_center.x, second_center.y);
    worldgen_controls_probe_record(
        report, "worldgen_controls_relief_crossed_coincident_geometry",
        crossed_ok && coincident_ok,
        "crossed_identity=%d coincident_x=%d independent_hit_centers=%d",
        crossed_ok, first_center.x == second_center.x, coincident_ok);
    return crossed_ok && coincident_ok;
}

static HWND make_probe_window(void) {
    return CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "STATIC",
                           "worldgen-controls-probe", WS_POPUP,
                           0, 0, 1280, 800, NULL, NULL,
                           GetModuleHandle(NULL), NULL);
}

static int drag_relief_handle(HWND hwnd, RECT client, int handle_index,
                              int target_value) {
    const UiWorldgenControlState *state;
    UiWorldgenPanelLayout layout;
    POINT start;
    int target_x;
    ui_worldgen_view_build(client, 460, &layout);
    ui_worldgen_control_state_set_scroll(
        UI_WORLDGEN_TAB_PHYSICAL,
        layout.tab_max_scroll[UI_WORLDGEN_TAB_PHYSICAL]);
    ui_worldgen_view_build(client, 460, &layout);
    start = rect_center(layout.physical.relief_handle_hit[handle_index]);
    target_x = ui_worldgen_panel_value_to_axis(
        target_value, 0, 100, layout.physical.relief_baseline.left,
        layout.physical.relief_baseline.right - 1);
    if (!ui_worldgen_input_mouse_down(hwnd, client, 460, start.x, start.y)) {
        return 0;
    }
    state = ui_worldgen_control_state_get();
    if (state->interaction.dragging.kind !=
            UI_WORLDGEN_CONTROL_RELIEF_HANDLE ||
        state->interaction.dragging.index != handle_index ||
        state->interaction.pressed.kind !=
            UI_WORLDGEN_CONTROL_RELIEF_HANDLE ||
        state->interaction.pressed.index != handle_index ||
        state->interaction.focused.kind !=
            UI_WORLDGEN_CONTROL_RELIEF_HANDLE ||
        state->interaction.focused.index != handle_index) return 0;
    ui_worldgen_input_mouse_move(hwnd, client, 460, target_x, start.y);
    ui_worldgen_input_mouse_up(hwnd, client, 460, target_x, start.y);
    state = ui_worldgen_control_state_get();
    return state->interaction.dragging.kind == UI_WORLDGEN_CONTROL_NONE &&
           state->interaction.pressed.kind == UI_WORLDGEN_CONTROL_NONE;
}

static int case_input_drag_release(WorldgenControlsProbeReport *report) {
    RECT client = {0, 0, 1280, 800};
    UiWorldgenPanelLayout layout;
    UiWorldgenInputDiagnostics input_diagnostics;
    UiWorldgenCommandDiagnostics command_diagnostics;
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig after;
    HWND hwnd = make_probe_window();
    int first_ok = 0;
    int second_ok = 0;
    int release_ok = 0;
    int hover_ok = 0;
    if (!hwnd) {
        worldgen_controls_probe_record(
            report, "worldgen_controls_input_drag_release", 0,
            "reason=hidden_window_create_failed error=%lu", GetLastError());
        return 0;
    }
    ui_worldgen_input_reset_diagnostics();
    ui_worldgen_command_reset_diagnostics();
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 50);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 50);
    first_ok = drag_relief_handle(hwnd, client, 0, 20) &&
               ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_RELIEF) == 20 &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_BIAS_MOUNTAIN) == 50;
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 50);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 50);
    second_ok = drag_relief_handle(hwnd, client, 1, 80) &&
                ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_RELIEF) == 50 &&
                ui_worldgen_config_get_field(
                    UI_WORLDGEN_FIELD_BIAS_MOUNTAIN) == 80;
    ui_worldgen_view_build(client, 460, &layout);
    {
        POINT hover = rect_center(layout.dice_button);
        ui_worldgen_input_mouse_move(hwnd, client, 460, hover.x, hover.y);
        hover_ok = ui_worldgen_control_state_get()->interaction.hovered.kind ==
                   UI_WORLDGEN_CONTROL_DICE;
        ui_worldgen_input_mouse_leave(hwnd);
        hover_ok &= ui_worldgen_control_state_get()->interaction.hovered.kind ==
                    UI_WORLDGEN_CONTROL_NONE;
    }
    ui_worldgen_config_read(&before);
    {
        POINT press = rect_center(layout.reset_button);
        ui_worldgen_input_mouse_down(hwnd, client, 460, press.x, press.y);
        ui_worldgen_input_mouse_up(hwnd, client, 460,
                                   layout.panel.left - 20,
                                   layout.panel.bottom - 20);
    }
    ui_worldgen_config_read(&after);
    ui_worldgen_input_get_diagnostics(&input_diagnostics);
    ui_worldgen_command_get_diagnostics(&command_diagnostics);
    release_ok = input_diagnostics.release_outside_cancels == 1 &&
                 command_diagnostics.reset_commands == 0 &&
                 ui_worldgen_config_equal(&before, &after) &&
                 ui_worldgen_control_state_get()->interaction.pressed.kind ==
                     UI_WORLDGEN_CONTROL_NONE;
    worldgen_controls_probe_record(
        report, "worldgen_controls_input_drag_release",
        first_ok && second_ok && release_ok && hover_ok,
        "relief_handle_access=%d mountain_handle_access=%d focus_pressed_drag=1 hover_leave=%d drag_updates=%u release_outside=%u release_cancel_ok=%d",
        first_ok, second_ok, hover_ok, input_diagnostics.drag_updates,
        input_diagnostics.release_outside_cancels, release_ok);
    ui_worldgen_control_state_clear_interaction();
    DestroyWindow(hwnd);
    return first_ok && second_ok && release_ok && hover_ok;
}

int worldgen_controls_probe_interaction(
    WorldgenControlsProbeReport *report) {
    int ok = 1;
    ok &= case_layout_widths_fixed_shell(report);
    ok &= case_checkpoint_geometry(report);
    ok &= case_axis_boundaries(report);
    ok &= case_panel_routing_boundary(report);
    ok &= case_relief_layout_identity(report);
    ok &= case_input_drag_release(report);
    return ok;
}
