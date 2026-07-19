#ifndef WORLD_SIM_RIVER_PRESENTATION_STATE_H
#define WORLD_SIM_RIVER_PRESENTATION_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "core/world_types.h"

extern RiverPath *river_paths;
extern int river_path_count;

int river_presentation_state_can_adopt(const RiverPath *owned_paths, int count,
                                       int map_w, int map_h);
void river_presentation_state_adopt_prevalidated(RiverPath **owned_paths,
                                                 int count);
int river_presentation_state_adopt(RiverPath **owned_paths, int count,
                                   int map_w, int map_h);
void river_presentation_state_clear(void);
size_t river_presentation_state_retained_bytes(void);
uint32_t river_presentation_state_revision(void);

#endif
