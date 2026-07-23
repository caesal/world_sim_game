#include "ui/ui_worldgen_random.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static unsigned int ui_worldgen_rng_state;

static unsigned int ui_worldgen_random_next(void) {
    LARGE_INTEGER counter;
    unsigned int value;
    if (!ui_worldgen_rng_state) {
        QueryPerformanceCounter(&counter);
        ui_worldgen_rng_state =
            (unsigned int)counter.LowPart ^ (unsigned int)counter.HighPart ^
            GetTickCount() ^ (GetCurrentProcessId() * 1103515245u);
        if (!ui_worldgen_rng_state) ui_worldgen_rng_state = 0x9e3779b9u;
    }
    value = ui_worldgen_rng_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    ui_worldgen_rng_state = value ? value : 0x85ebca6bu;
    return ui_worldgen_rng_state;
}

int ui_worldgen_random_range(int min_value, int max_value) {
    unsigned int width;
    if (max_value <= min_value) return min_value;
    width = (unsigned int)(max_value - min_value + 1);
    return min_value + (int)(ui_worldgen_random_next() % width);
}

static int random_slider_value(int old_value) {
    int value = ui_worldgen_random_range(5, 94);
    return value >= old_value ? value + 1 : value;
}

static UiWorldgenFieldMask randomize_field(UiWorldgenConfigField field) {
    int old_value = ui_worldgen_config_get_field(field);
    int new_value = random_slider_value(old_value);
    if (!ui_worldgen_config_set_field(field, new_value)) return 0;
    return UI_WORLDGEN_FIELD_MASK(field);
}

UiWorldgenFieldMask ui_worldgen_randomize_physical(void) {
    UiWorldgenFieldMask changed = 0;
    changed |= randomize_field(UI_WORLDGEN_FIELD_OCEAN);
    changed |= randomize_field(UI_WORLDGEN_FIELD_CONTINENT);
    changed |= randomize_field(UI_WORLDGEN_FIELD_RELIEF);
    changed |= randomize_field(UI_WORLDGEN_FIELD_MOISTURE);
    changed |= randomize_field(UI_WORLDGEN_FIELD_DROUGHT);
    changed |= randomize_field(UI_WORLDGEN_FIELD_VEGETATION);
    return changed;
}

UiWorldgenFieldMask ui_worldgen_randomize_advanced(void) {
    UiWorldgenFieldMask changed = 0;
    changed |= randomize_field(UI_WORLDGEN_FIELD_BIAS_FOREST);
    changed |= randomize_field(UI_WORLDGEN_FIELD_BIAS_DESERT);
    changed |= randomize_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN);
    changed |= randomize_field(UI_WORLDGEN_FIELD_BIAS_WETLAND);
    changed |= randomize_field(UI_WORLDGEN_FIELD_REGION_SIZE);
    return changed;
}

UiWorldgenFieldMask ui_worldgen_randomize_all(void) {
    UiWorldgenFieldMask changed = ui_worldgen_randomize_physical();
    return changed | ui_worldgen_randomize_advanced();
}

void ui_worldgen_random_validation_set_state(unsigned int state) {
    ui_worldgen_rng_state = state ? state : 0x9e3779b9u;
}

