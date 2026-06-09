#include "render/profiling_switches.h"

static int profiling_switches[PROFILING_SWITCH_COUNT] = {
    1, 1, 1, 1, 1, 1, 1, 1
};

int profiling_switch_enabled(int switch_id) {
    if (switch_id < 0 || switch_id >= PROFILING_SWITCH_COUNT) return 1;
    return profiling_switches[switch_id] != 0;
}

void profiling_switch_toggle(int switch_id) {
    if (switch_id < 0 || switch_id >= PROFILING_SWITCH_COUNT) return;
    profiling_switches[switch_id] = !profiling_switches[switch_id];
}
