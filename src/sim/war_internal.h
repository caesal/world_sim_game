#ifndef WORLD_SIM_WAR_INTERNAL_H
#define WORLD_SIM_WAR_INTERNAL_H

#include "sim/war.h"

#define MAX_ACTIVE_WARS WAR_SAVE_SLOT_COUNT

extern ActiveWar active_wars[MAX_ACTIVE_WARS];
extern int support_casualties[MAX_CIVS];
extern int total_started_wars;

#endif
