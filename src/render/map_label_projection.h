#ifndef WORLD_SIM_MAP_LABEL_PROJECTION_H
#define WORLD_SIM_MAP_LABEL_PROJECTION_H

#include "ui/ui_layout.h"

int map_label_projection_anchor_x(MapLayout layout, int map_w, int anchor_x2);
int map_label_projection_anchor_y(MapLayout layout, int map_h, int anchor_y2);
RECT map_label_projection_rect(int x, int y, SIZE size, int pad);
int map_label_projection_rect_visible(RECT rect, RECT viewport);
int map_label_projection_slot_open(const RECT *used, int used_count, RECT candidate);
void map_label_projection_slot_position(int centered, SIZE size, int anchor_x,
                                        int anchor_y, int slot, int *x, int *y);

#endif
