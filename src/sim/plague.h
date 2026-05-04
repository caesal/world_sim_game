#ifndef WORLD_SIM_PLAGUE_H
#define WORLD_SIM_PLAGUE_H

#include "core/game_types.h"

typedef struct {
    int active;
    int infected;
    int severity;
    int months_left;
    int immunity;
    int deaths_total;
    int origin_city;
    int age_months;
} PlagueState;

void plague_reset(void);
void plague_update_month(void);
int plague_seed_random_outbreak(void);
int plague_seed_city(int city_id, int severity, int months);
void plague_notify_migration(int from_city, int to_city, int migrants);
void plague_notify_war_casualties(int civ_id, int casualties);

int plague_city_active(int city_id);
int plague_city_severity(int city_id);
int plague_city_deaths_total(int city_id);
int plague_city_months_left(int city_id);
int plague_tile_severity(int x, int y);
int plague_civ_active_count(int civ_id);
int plague_civ_pressure(int civ_id);
int plague_civ_deaths_total(int civ_id);
int plague_civ_peak_severity(int civ_id);
int plague_civ_months_left(int civ_id);
int plague_route_exposure(int route_id);

#endif
