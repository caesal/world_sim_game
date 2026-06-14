#include "sim/war.h"

#include "sim/war_internal.h"

int war_has_empty_slot(void) {
    int i;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) return 1;
    }
    return 0;
}
