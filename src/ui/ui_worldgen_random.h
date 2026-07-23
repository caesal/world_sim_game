#ifndef WORLD_SIM_UI_WORLDGEN_RANDOM_H
#define WORLD_SIM_UI_WORLDGEN_RANDOM_H

#include "ui/ui_worldgen_config_adapter.h"

int ui_worldgen_random_range(int min_value, int max_value);
UiWorldgenFieldMask ui_worldgen_randomize_physical(void);
UiWorldgenFieldMask ui_worldgen_randomize_advanced(void);
UiWorldgenFieldMask ui_worldgen_randomize_all(void);
void ui_worldgen_random_validation_set_state(unsigned int state);

#endif
