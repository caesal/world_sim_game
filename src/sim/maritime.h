#ifndef WORLD_SIM_MARITIME_H
#define WORLD_SIM_MARITIME_H

#include "core/game_types.h"

void maritime_reset(void);
void maritime_rebuild_routes(void);
void maritime_update_migration(void);
int maritime_route_between_cities(int city_a, int city_b, int *distance);
int maritime_has_contact(int civ_a, int civ_b);
int maritime_trade_bonus(int civ_a, int civ_b);
void maritime_try_overseas_expansion(int civ_id, int resource_score, char *log, size_t log_size);

#endif
