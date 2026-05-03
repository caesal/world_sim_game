#ifndef WORLD_SIM_PORTS_H
#define WORLD_SIM_PORTS_H

#include "../core/game_state.h"

void ports_reset_regions(void);
void ports_maybe_make_city_port(int city_id);
void ports_update_migration(void);

#endif
