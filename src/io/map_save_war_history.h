#ifndef WORLD_SIM_MAP_SAVE_WAR_HISTORY_H
#define WORLD_SIM_MAP_SAVE_WAR_HISTORY_H

#include "sim/war_history_types.h"

#include <stdio.h>

#define MAP_SAVE_WAR_HISTORY_BLOCK_VERSION 1

typedef struct {
    WarHistorySaveState *state;
    int ready;
} MapSaveWarHistoryStage;

int map_save_war_history_write(FILE *file);
int map_save_war_history_stage_read(FILE *file, int save_version,
                                    int save_year, int save_month,
                                    int saved_civ_count,
                                    MapSaveWarHistoryStage *stage);
const WarHistorySaveState *map_save_war_history_stage_state(
    const MapSaveWarHistoryStage *stage);
int map_save_war_history_stage_commit(const MapSaveWarHistoryStage *stage);
void map_save_war_history_stage_release(MapSaveWarHistoryStage *stage);

#endif
