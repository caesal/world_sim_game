#include "ui/ui_worldgen_control_state.h"

#include <stddef.h>
#include <string.h>

static UiWorldgenControlState control_state;
static UiWorldgenEffectiveConfig observed_config;
static int has_observed_config;

static UiWorldgenControlTarget no_target(void) {
    UiWorldgenControlTarget target;
    target.kind = UI_WORLDGEN_CONTROL_NONE;
    target.index = -1;
    return target;
}

static int target_equal(UiWorldgenControlTarget left,
                        UiWorldgenControlTarget right) {
    return left.kind == right.kind && left.index == right.index;
}

static UiWorldgenControlTarget normalize_target(
    UiWorldgenControlTarget target) {
    if (target.kind == UI_WORLDGEN_CONTROL_NONE) return no_target();
    return target;
}

static int corners_equal(const UiWorldgenClimateCorners *left,
                         const UiWorldgenClimateCorners *right) {
    return left->top_left.x == right->top_left.x &&
           left->top_left.y == right->top_left.y &&
           left->top_right.x == right->top_right.x &&
           left->top_right.y == right->top_right.y &&
           left->bottom_right.x == right->bottom_right.x &&
           left->bottom_right.y == right->bottom_right.y &&
           left->bottom_left.x == right->bottom_left.x &&
           left->bottom_left.y == right->bottom_left.y;
}

static void bump_revision(void) {
    control_state.revision++;
    if (!control_state.revision) control_state.revision = 1;
}

static void reset_interaction(void) {
    UiWorldgenControlTarget none = no_target();
    control_state.interaction.hovered = none;
    control_state.interaction.pressed = none;
    control_state.interaction.focused = none;
    control_state.interaction.dragging = none;
}

static int clear_interaction_values(void) {
    UiWorldgenControlTarget none = no_target();
    int changed = !target_equal(control_state.interaction.hovered, none) ||
                  !target_equal(control_state.interaction.pressed, none) ||
                  !target_equal(control_state.interaction.focused, none) ||
                  !target_equal(control_state.interaction.dragging, none);
    reset_interaction();
    return changed;
}

static void record_observed(const UiWorldgenEffectiveConfig *config) {
    observed_config = *config;
    has_observed_config = 1;
}

static void update_preset_and_dirty(
    const UiWorldgenEffectiveConfig *config) {
    control_state.preset = ui_worldgen_config_is_balanced(config) ?
        UI_WORLDGEN_PRESET_BALANCED : UI_WORLDGEN_PRESET_CUSTOM;
    control_state.dirty = control_state.generation_failed ||
        !control_state.has_applied_config ||
        !ui_worldgen_config_equal(config, &control_state.applied_config);
}

static void rebuild_decomposition(
    const UiWorldgenEffectiveConfig *config) {
    ui_worldgen_climate_corners_from_config(config,
                                             &control_state.climate_corners);
    control_state.region_category =
        ui_worldgen_region_category_for_value(config->pending_map_size,
                                              config->region_size_slider);
}

static void initialize_from_config(const UiWorldgenEffectiveConfig *config,
                                   int mark_applied) {
    memset(&control_state, 0, sizeof(control_state));
    control_state.initialized = 1;
    control_state.tab = UI_WORLDGEN_TAB_PHYSICAL;
    reset_interaction();
    rebuild_decomposition(config);
    record_observed(config);
    if (mark_applied) {
        control_state.applied_config = *config;
        control_state.has_applied_config = 1;
    }
    update_preset_and_dirty(config);
    control_state.revision = 1;
}

void ui_worldgen_control_state_init_fresh(void) {
    UiWorldgenEffectiveConfig config;
    ui_worldgen_config_apply_balanced();
    ui_worldgen_config_read(&config);
    initialize_from_config(&config, 1);
}

static void ensure_initialized(void) {
    if (!control_state.initialized) ui_worldgen_control_state_init_fresh();
}

int ui_worldgen_control_state_resync(UiWorldgenResyncMode mode) {
    UiWorldgenEffectiveConfig config;
    int config_changed;
    int state_changed = 0;
    ui_worldgen_config_read(&config);
    if (!control_state.initialized) {
        initialize_from_config(
            &config, mode == UI_WORLDGEN_RESYNC_MARK_APPLIED);
        return 1;
    }
    config_changed = !has_observed_config ||
        !ui_worldgen_config_equal(&config, &observed_config);
    if (config_changed) {
        rebuild_decomposition(&config);
        record_observed(&config);
        state_changed = 1;
    }
    if (mode == UI_WORLDGEN_RESYNC_MARK_APPLIED) {
        if (!control_state.has_applied_config ||
            !ui_worldgen_config_equal(&config,
                                      &control_state.applied_config) ||
            control_state.generation_failed || control_state.dirty) {
            state_changed = 1;
        }
        control_state.applied_config = config;
        control_state.has_applied_config = 1;
        control_state.generation_failed = 0;
    }
    update_preset_and_dirty(&config);
    if (state_changed) bump_revision();
    return state_changed;
}

static void sync_before_mutation(void) {
    UiWorldgenEffectiveConfig config;
    ensure_initialized();
    ui_worldgen_config_read(&config);
    if (!has_observed_config ||
        !ui_worldgen_config_equal(&config, &observed_config)) {
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    }
}

static void note_effective_write(const UiWorldgenEffectiveConfig *config,
                                 int rebuild_climate,
                                 int rebuild_region) {
    if (rebuild_climate) {
        ui_worldgen_climate_corners_from_config(
            config, &control_state.climate_corners);
    }
    if (rebuild_region) {
        control_state.region_category =
            ui_worldgen_region_category_for_value(
                config->pending_map_size, config->region_size_slider);
    }
    record_observed(config);
    update_preset_and_dirty(config);
    bump_revision();
}

const UiWorldgenControlState *ui_worldgen_control_state_get(void) {
    return &control_state;
}

void ui_worldgen_control_state_mark_applied(void) {
    UiWorldgenEffectiveConfig config;
    int changed;
    sync_before_mutation();
    ui_worldgen_config_read(&config);
    changed = !control_state.has_applied_config ||
        !ui_worldgen_config_equal(&config, &control_state.applied_config) ||
        control_state.generation_failed || control_state.dirty;
    control_state.applied_config = config;
    control_state.has_applied_config = 1;
    control_state.generation_failed = 0;
    record_observed(&config);
    update_preset_and_dirty(&config);
    if (changed) bump_revision();
}

void ui_worldgen_control_state_mark_generation_failed(void) {
    sync_before_mutation();
    if (control_state.generation_failed && control_state.dirty) return;
    control_state.generation_failed = 1;
    control_state.dirty = 1;
    bump_revision();
}

int ui_worldgen_control_state_set_tab(UiWorldgenControlTab tab) {
    sync_before_mutation();
    if (tab < UI_WORLDGEN_TAB_PHYSICAL || tab >= UI_WORLDGEN_TAB_COUNT ||
        control_state.tab == tab) return 0;
    control_state.tab = tab;
    clear_interaction_values();
    bump_revision();
    return 1;
}

int ui_worldgen_control_state_set_scroll(UiWorldgenControlTab tab,
                                         int scroll_offset) {
    sync_before_mutation();
    if (tab < UI_WORLDGEN_TAB_PHYSICAL || tab >= UI_WORLDGEN_TAB_COUNT) {
        return 0;
    }
    if (scroll_offset < 0) scroll_offset = 0;
    if (control_state.scroll_offsets[tab] == scroll_offset) return 0;
    control_state.scroll_offsets[tab] = scroll_offset;
    bump_revision();
    return 1;
}

static int climate_field(UiWorldgenConfigField field) {
    return field == UI_WORLDGEN_FIELD_MOISTURE ||
           field == UI_WORLDGEN_FIELD_DROUGHT ||
           field == UI_WORLDGEN_FIELD_BIAS_FOREST ||
           field == UI_WORLDGEN_FIELD_BIAS_DESERT;
}

static int set_map_size(int map_size) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenRegionCategory category;
    int changed;
    int region_value;
    sync_before_mutation();
    category = control_state.region_category;
    changed = ui_worldgen_config_set_field(
        UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, map_size);
    if (!changed) return 0;
    ui_worldgen_config_read(&config);
    if (category != UI_WORLDGEN_REGION_CUSTOM) {
        region_value = ui_worldgen_region_preset_value(
            config.pending_map_size, category);
        ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_REGION_SIZE,
                                     region_value);
        ui_worldgen_config_read(&config);
    }
    record_observed(&config);
    update_preset_and_dirty(&config);
    bump_revision();
    return 1;
}

int ui_worldgen_control_state_set_field(UiWorldgenConfigField field,
                                        int value) {
    UiWorldgenEffectiveConfig config;
    if (field == UI_WORLDGEN_FIELD_PENDING_MAP_SIZE) {
        return set_map_size(value);
    }
    if (field == UI_WORLDGEN_FIELD_REGION_SIZE) {
        return ui_worldgen_control_state_set_region_custom_value(value);
    }
    sync_before_mutation();
    if (!ui_worldgen_config_set_field(field, value)) return 0;
    ui_worldgen_config_read(&config);
    note_effective_write(&config, climate_field(field), 0);
    return 1;
}

static UiWorldgenPoint *corner_address(UiWorldgenClimateCornerId corner) {
    switch (corner) {
        case UI_WORLDGEN_CLIMATE_TOP_LEFT:
            return &control_state.climate_corners.top_left;
        case UI_WORLDGEN_CLIMATE_TOP_RIGHT:
            return &control_state.climate_corners.top_right;
        case UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT:
            return &control_state.climate_corners.bottom_right;
        case UI_WORLDGEN_CLIMATE_BOTTOM_LEFT:
            return &control_state.climate_corners.bottom_left;
        default:
            return NULL;
    }
}

int ui_worldgen_control_state_set_climate_corner(
    UiWorldgenClimateCornerId corner, int x, int y) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenPoint next;
    UiWorldgenPoint *current;
    sync_before_mutation();
    current = corner_address(corner);
    if (!current) return 0;
    next.x = x;
    next.y = y;
    ui_worldgen_climate_clamp_corner(corner, &next);
    if (current->x == next.x && current->y == next.y) return 0;
    *current = next;
    ui_worldgen_config_read(&config);
    ui_worldgen_climate_corners_to_config(&control_state.climate_corners,
                                          &config);
    if (corner == UI_WORLDGEN_CLIMATE_TOP_LEFT ||
        corner == UI_WORLDGEN_CLIMATE_BOTTOM_LEFT) {
        ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_FOREST,
                                     config.bias_forest_slider);
    } else {
        ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_DESERT,
                                     config.bias_desert_slider);
    }
    if (corner == UI_WORLDGEN_CLIMATE_TOP_LEFT ||
        corner == UI_WORLDGEN_CLIMATE_TOP_RIGHT) {
        ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_MOISTURE,
                                     config.moisture_slider);
    } else {
        ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_DROUGHT,
                                     config.drought_slider);
    }
    ui_worldgen_config_read(&config);
    note_effective_write(&config, 0, 0);
    return 1;
}

int ui_worldgen_control_state_set_region_category(
    UiWorldgenRegionCategory category) {
    UiWorldgenEffectiveConfig config;
    int raw_value;
    int config_changed;
    int state_changed;
    sync_before_mutation();
    if (category < UI_WORLDGEN_REGION_VERY_SMALL ||
        category >= UI_WORLDGEN_REGION_CATEGORY_COUNT) return 0;
    if (category == UI_WORLDGEN_REGION_CUSTOM) {
        if (control_state.region_category == category) return 0;
        control_state.region_category = category;
        bump_revision();
        return 1;
    }
    ui_worldgen_config_read(&config);
    raw_value = ui_worldgen_region_preset_value(config.pending_map_size,
                                                category);
    if (raw_value < 0) return 0;
    config_changed = ui_worldgen_config_set_field(
        UI_WORLDGEN_FIELD_REGION_SIZE, raw_value);
    state_changed = control_state.region_category != category;
    if (!config_changed && !state_changed) return 0;
    control_state.region_category = category;
    ui_worldgen_config_read(&config);
    note_effective_write(&config, 0, 0);
    return 1;
}

int ui_worldgen_control_state_set_region_custom_value(int raw_value) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenRegionCategory category;
    int config_changed;
    int state_changed;
    sync_before_mutation();
    config_changed = ui_worldgen_config_set_field(
        UI_WORLDGEN_FIELD_REGION_SIZE, raw_value);
    ui_worldgen_config_read(&config);
    category = ui_worldgen_region_category_for_value(
        config.pending_map_size, config.region_size_slider);
    state_changed = control_state.region_category != category;
    if (!config_changed && !state_changed) return 0;
    control_state.region_category = category;
    note_effective_write(&config, 0, 0);
    return 1;
}

int ui_worldgen_control_state_apply_balanced(void) {
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig after;
    UiWorldgenClimateCorners old_corners;
    UiWorldgenRegionCategory old_region;
    UiWorldgenPreset old_preset;
    int changed;
    sync_before_mutation();
    ui_worldgen_config_read(&before);
    old_corners = control_state.climate_corners;
    old_region = control_state.region_category;
    old_preset = control_state.preset;
    ui_worldgen_config_apply_balanced();
    ui_worldgen_config_read(&after);
    rebuild_decomposition(&after);
    record_observed(&after);
    update_preset_and_dirty(&after);
    changed = !ui_worldgen_config_equal(&before, &after) ||
        !corners_equal(&old_corners, &control_state.climate_corners) ||
        old_region != control_state.region_category ||
        old_preset != control_state.preset;
    if (changed) bump_revision();
    return changed;
}

static int set_interaction_target(UiWorldgenControlTarget *slot,
                                  UiWorldgenControlTarget target) {
    target = normalize_target(target);
    if (target_equal(*slot, target)) return 0;
    *slot = target;
    bump_revision();
    return 1;
}

int ui_worldgen_control_state_set_hovered(UiWorldgenControlTarget target) {
    ensure_initialized();
    return set_interaction_target(&control_state.interaction.hovered, target);
}

int ui_worldgen_control_state_set_pressed(UiWorldgenControlTarget target) {
    ensure_initialized();
    return set_interaction_target(&control_state.interaction.pressed, target);
}

int ui_worldgen_control_state_set_focused(UiWorldgenControlTarget target) {
    ensure_initialized();
    return set_interaction_target(&control_state.interaction.focused, target);
}

int ui_worldgen_control_state_begin_drag(UiWorldgenControlTarget target) {
    int changed;
    ensure_initialized();
    target = normalize_target(target);
    changed = !target_equal(control_state.interaction.dragging, target) ||
              !target_equal(control_state.interaction.pressed, target);
    if (!changed) return 0;
    control_state.interaction.dragging = target;
    control_state.interaction.pressed = target;
    bump_revision();
    return 1;
}

int ui_worldgen_control_state_end_drag(void) {
    UiWorldgenControlTarget none = no_target();
    int changed;
    ensure_initialized();
    changed = !target_equal(control_state.interaction.dragging, none) ||
              !target_equal(control_state.interaction.pressed, none);
    if (!changed) return 0;
    control_state.interaction.dragging = none;
    control_state.interaction.pressed = none;
    bump_revision();
    return 1;
}

int ui_worldgen_control_state_clear_interaction(void) {
    int changed;
    ensure_initialized();
    changed = clear_interaction_values();
    if (changed) bump_revision();
    return changed;
}
