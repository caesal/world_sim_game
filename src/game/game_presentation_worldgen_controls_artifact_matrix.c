#include "game/game_presentation_worldgen_controls_artifact_internal.h"
#include "game/game_presentation_worldgen_initial_civs_probe.h"

#include "core/constants.h"
#include "core/game_types.h"
#include "ui/ui_forms.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_view.h"

#include <stdio.h>
#include <string.h>

static const char *tab_slug(UiWorldgenControlTab tab) {
    static const char *names[UI_WORLDGEN_TAB_COUNT] = {
        "physical", "climate", "hydrology_regions", "legacy"
    };
    return names[tab];
}

static UiWorldgenControlTarget target(UiWorldgenControlKind kind,
                                      int index) {
    UiWorldgenControlTarget value = {kind, index};
    return value;
}

static int text_equals(HWND control, const char *expected) {
    char text[32];
    if (!control || !expected) return 0;
    GetWindowTextA(control, text, sizeof(text));
    return strcmp(text, expected) == 0;
}

static int require_native_mask(WorldgenControlsArtifactWriter *writer,
                               const char *fixture,
                               uint32_t required_mask) {
    int ok = writer &&
             (writer->last_native_mask & required_mask) == required_mask &&
             writer->last_native_visible == writer->last_native_composited;
    if (!writer || !writer->manifest) return 0;
    fprintf(writer->manifest,
            "native_fixture=%s ok=%d required_mask=%08x visible_mask=%08x visible=%u composited=%u\n",
            fixture, ok, (unsigned int)required_mask,
            (unsigned int)writer->last_native_mask,
            writer->last_native_visible, writer->last_native_composited);
    if (!ok) writer->failure_count++;
    return ok;
}

static void reset_presentation_state(UiWorldgenControlTab tab) {
    int index;
    ui_worldgen_control_state_apply_balanced();
    ui_worldgen_control_state_mark_applied();
    ui_worldgen_control_state_clear_interaction();
    for (index = 0; index < UI_WORLDGEN_TAB_COUNT; index++) {
        ui_worldgen_control_state_set_scroll((UiWorldgenControlTab)index, 0);
    }
    ui_worldgen_control_state_set_tab(tab);
    hover_x = -1;
    hover_y = -1;
}

static void prepare_custom(UiWorldgenControlTab tab) {
    reset_presentation_state(tab);
    ui_worldgen_control_state_set_field(
        UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, MAP_SIZE_LARGE);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_OCEAN, 64);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_CONTINENT, 31);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 72);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_MOISTURE, 68);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_DROUGHT, 27);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_VEGETATION, 76);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 39);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_WETLAND, 83);
    ui_worldgen_control_state_set_region_custom_value(49);
}

static void set_scroll_fraction(int width, UiWorldgenControlTab tab,
                                int numerator, int denominator) {
    UiWorldgenPanelLayout layout;
    RECT client = {0, 0, width, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT};
    ui_worldgen_control_state_set_tab(tab);
    ui_worldgen_control_state_set_scroll(tab, 0);
    ui_worldgen_view_build(client, width, &layout);
    ui_worldgen_control_state_set_scroll(
        tab, layout.tab_max_scroll[tab] * numerator / denominator);
}

static int render_base_matrix(WorldgenControlsArtifactWriter *writer) {
    static const int widths[] = {340, 460};
    static const int languages[] = {UI_LANG_EN, UI_LANG_ZH};
    static const char *language_slugs[] = {"en", "zh"};
    static const char *preset_slugs[] = {"balanced", "custom"};
    char filename[128];
    int ok = 1;
    int preset;
    int tab;
    int language;
    int width;
    for (preset = 0; preset < 2; preset++) {
        for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
            for (language = 0; language < 2; language++) {
                for (width = 0; width < 2; width++) {
                    if (preset == 0) {
                        reset_presentation_state((UiWorldgenControlTab)tab);
                    } else {
                        prepare_custom((UiWorldgenControlTab)tab);
                    }
                    snprintf(filename, sizeof(filename),
                             "worldgen_%s_%s_%d_%s.bmp",
                             tab_slug((UiWorldgenControlTab)tab),
                             language_slugs[language], widths[width],
                             preset_slugs[preset]);
                    ok &= worldgen_controls_artifact_render(
                        writer, filename, widths[width], languages[language]);
                }
            }
        }
    }
    return ok;
}

static int render_relief_climate_river(
    WorldgenControlsArtifactWriter *writer) {
    int ok = 1;
    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 20);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 80);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_PHYSICAL, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_physical_en_460_relief_crossed.bmp", 460,
        UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 50);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 50);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_PHYSICAL, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_physical_en_460_relief_coincident.bmp", 460,
        UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_LEFT, -41, 37);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_RIGHT, 12, 48);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT, 45, -7);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_LEFT, -8, -33);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_CLIMATE, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_irregular.bmp", 460, UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_LEFT, -50, 50);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_RIGHT, 50, 50);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT, 50, -50);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_LEFT, -50, -50);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_CLIMATE, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_boundary.bmp", 460, UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_BIAS_WETLAND, 63);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 2);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_hydrology_en_460_river_63.bmp", 460, UI_LANG_EN);
    return ok;
}

static void set_climate_points(const int points[][2]) {
    int index;
    for (index = 0; index < UI_WORLDGEN_CLIMATE_CORNER_COUNT; index++) {
        ui_worldgen_control_state_set_climate_corner(
            (UiWorldgenClimateCornerId)index,
            points[index][0], points[index][1]);
    }
}

static int render_climate_review_states(
    WorldgenControlsArtifactWriter *writer) {
    static const int irregular[UI_WORLDGEN_CLIMATE_CORNER_COUNT][2] = {
        {-41, 37}, {12, 48}, {45, -7}, {-8, -33}
    };
    static const int crossed[UI_WORLDGEN_CLIMATE_CORNER_COUNT][2] = {
        {0, 50}, {50, 0}, {0, -50}, {-50, 0}
    };
    static const int overlapped[UI_WORLDGEN_CLIMATE_CORNER_COUNT][2] = {
        {0, 0}, {0, 0}, {0, 0}, {0, 0}
    };
    int ok = 1;
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    set_climate_points(irregular);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_340_irregular.bmp", 340, UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    set_climate_points(crossed);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_zh_340_boundary_crossed.bmp", 340,
        UI_LANG_ZH);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_set_hovered(target(
        UI_WORLDGEN_CONTROL_CLIMATE_CORNER, UI_WORLDGEN_CLIMATE_TOP_LEFT));
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_hover_top_left.bmp", 460,
        UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_begin_drag(target(
        UI_WORLDGEN_CONTROL_CLIMATE_CORNER, UI_WORLDGEN_CLIMATE_TOP_RIGHT));
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_drag_top_right.bmp", 460,
        UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    set_climate_points(overlapped);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_overlap_identity.bmp", 460,
        UI_LANG_EN);
    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_VEGETATION, 76);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_climate_en_460_vegetation_full.bmp", 460,
        UI_LANG_EN);
    return ok;
}

static int render_region_matrix(WorldgenControlsArtifactWriter *writer) {
    static const char *names[UI_WORLDGEN_REGION_PRESET_COUNT] = {
        "very_small", "small", "medium", "large", "very_large"
    };
    char filename[128];
    int ok = 1;
    int category;
    for (category = 0; category < UI_WORLDGEN_REGION_PRESET_COUNT;
         category++) {
        reset_presentation_state(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
        ui_worldgen_control_state_set_region_category(
            (UiWorldgenRegionCategory)category);
        set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 1);
        snprintf(filename, sizeof(filename),
                 "worldgen_regions_en_460_%s.bmp", names[category]);
        ok &= worldgen_controls_artifact_render(
            writer, filename, 460, UI_LANG_EN);
    }
    reset_presentation_state(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_control_state_set_region_custom_value(49);
    ui_worldgen_control_state_set_region_category(UI_WORLDGEN_REGION_CUSTOM);
    ui_worldgen_control_state_set_focused(
        target(UI_WORLDGEN_CONTROL_REGION_CUSTOM_INPUT, 0));
    set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_regions_en_460_custom_49.bmp", 460, UI_LANG_EN);
    ok &= require_native_mask(
        writer, "region_custom_49",
        WORLDGEN_CONTROLS_NATIVE_REGION_CUSTOM);
    return ok;
}

static int render_legacy_positions(WorldgenControlsArtifactWriter *writer) {
    static const char *names[] = {"top", "middle", "bottom"};
    static const int numerators[] = {0, 1, 1};
    static const int denominators[] = {1, 2, 1};
    char filename[96];
    int ok = 1;
    int index;
    for (index = 0; index < 3; index++) {
        reset_presentation_state(UI_WORLDGEN_TAB_LEGACY);
        set_scroll_fraction(460, UI_WORLDGEN_TAB_LEGACY,
                            numerators[index], denominators[index]);
        snprintf(filename, sizeof(filename),
                 "worldgen_legacy_en_460_%s.bmp", names[index]);
        ok &= worldgen_controls_artifact_render(
            writer, filename, 460, UI_LANG_EN);
        if (index == 2) {
            ok &= require_native_mask(
                writer, "legacy_bottom",
                WORLDGEN_CONTROLS_NATIVE_LEGACY_BOTTOM);
        }
    }
    return ok;
}

static int render_interaction_states(
    WorldgenControlsArtifactWriter *writer) {
    UiWorldgenPanelLayout layout;
    RECT client = {0, 0, 460, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT};
    int ok = 1;
    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    ui_worldgen_control_state_set_hovered(
        target(UI_WORLDGEN_CONTROL_DICE, 0));
    ui_worldgen_view_build(client, 460, &layout);
    hover_x = (layout.dice_button.left + layout.dice_button.right) / 2;
    hover_y = (layout.dice_button.top + layout.dice_button.bottom) / 2;
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_hover_dice.bmp", 460, UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    ui_worldgen_control_state_set_pressed(
        target(UI_WORLDGEN_CONTROL_RESET, 0));
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_pressed_reset.bmp", 460, UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_CLIMATE);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_selected_climate.bmp", 460,
        UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_control_state_set_region_custom_value(49);
    ui_worldgen_control_state_set_region_category(UI_WORLDGEN_REGION_CUSTOM);
    ui_worldgen_control_state_set_focused(
        target(UI_WORLDGEN_CONTROL_REGION_CUSTOM_INPUT, 0));
    set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_focused_region_custom.bmp", 460,
        UI_LANG_EN);
    ok &= require_native_mask(
        writer, "focused_region_custom",
        WORLDGEN_CONTROLS_NATIVE_REGION_CUSTOM);

    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    set_scroll_fraction(460, UI_WORLDGEN_TAB_PHYSICAL, 1, 2);
    ui_worldgen_control_state_begin_drag(
        target(UI_WORLDGEN_CONTROL_PHYSICAL_XY, 0));
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_drag_physical_xy.bmp", 460,
        UI_LANG_EN);

    reset_presentation_state(UI_WORLDGEN_TAB_PHYSICAL);
    ui_worldgen_control_state_set_pressed(
        target(UI_WORLDGEN_CONTROL_RESET, 0));
    ui_worldgen_control_state_clear_interaction();
    ok &= worldgen_controls_artifact_render(
        writer, "worldgen_interaction_release_outside.bmp", 460,
        UI_LANG_EN);
    return ok;
}

static int render_fixed_shell_scrolls(
    WorldgenControlsArtifactWriter *writer) {
    char filename[112];
    int ok = 1;
    int tab;
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        prepare_custom((UiWorldgenControlTab)tab);
        set_scroll_fraction(460, (UiWorldgenControlTab)tab, 1, 1);
        snprintf(filename, sizeof(filename),
                 "worldgen_fixed_shell_%s_scrolled.bmp",
                 tab_slug((UiWorldgenControlTab)tab));
        ok &= worldgen_controls_artifact_render(
            writer, filename, 460, UI_LANG_EN);
    }
    return ok;
}

static int render_initial_civs_sync(
    WorldgenControlsArtifactWriter *writer) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    int ok = 1;
    reset_presentation_state(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_forms_write_world_setup_controls();
    SetWindowTextA(form->hydrology_initial_civs_edit, "42");
    ok &= ui_forms_handle_metric_change(ID_HYDROLOGY_INITIAL_CIVS_EDIT);
    ok &= ui_worldgen_config_get_field(
              UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 42 &&
          text_equals(form->initial_civs_edit, "42") &&
          text_equals(form->hydrology_initial_civs_edit, "42");
    set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "hydrology_initial_civs_42_zh_460.bmp", 460, UI_LANG_ZH);
    ok &= require_native_mask(
        writer, "hydrology_initial_civs_42",
        WORLDGEN_CONTROLS_NATIVE_HYDROLOGY_INITIAL);

    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_control_state_set_scroll(UI_WORLDGEN_TAB_LEGACY, 0);
    ok &= worldgen_controls_artifact_render(
        writer, "legacy_initial_civs_42_zh_460.bmp", 460, UI_LANG_ZH);
    ok &= require_native_mask(
        writer, "legacy_initial_civs_42", WORLDGEN_CONTROLS_NATIVE_INITIAL);

    SetWindowTextA(form->initial_civs_edit, "73");
    ok &= ui_forms_handle_metric_change(ID_INITIAL_CIVS_EDIT);
    ok &= ui_worldgen_config_get_field(
              UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
          text_equals(form->hydrology_initial_civs_edit, "73");
    set_scroll_fraction(460, UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, 1, 1);
    ok &= worldgen_controls_artifact_render(
        writer, "hydrology_after_legacy_73_en_460.bmp", 460, UI_LANG_EN);
    ok &= require_native_mask(
        writer, "hydrology_after_legacy_73",
        WORLDGEN_CONTROLS_NATIVE_HYDROLOGY_INITIAL);
    return ok;
}

int worldgen_controls_artifact_matrix(
    WorldgenControlsArtifactWriter *writer) {
    int ok = 1;
    ok &= render_base_matrix(writer);
    ok &= render_relief_climate_river(writer);
    ok &= render_climate_review_states(writer);
    ok &= render_region_matrix(writer);
    ok &= render_legacy_positions(writer);
    ok &= render_interaction_states(writer);
    ok &= render_fixed_shell_scrolls(writer);
    ok &= render_initial_civs_sync(writer);
    ok &= worldgen_initial_civs_artifact_matrix(writer);
    return ok;
}
