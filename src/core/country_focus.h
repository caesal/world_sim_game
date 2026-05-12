#ifndef WORLD_SIM_COUNTRY_FOCUS_H
#define WORLD_SIM_COUNTRY_FOCUS_H

int country_bounds(int civ_id, int *min_x, int *min_y, int *max_x, int *max_y);
int country_focus_point(int civ_id, int *x, int *y);
void country_focus_invalidate(void);

#endif
