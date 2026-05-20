#ifndef WORLD_SIM_DIPLOMACY_BORDERS_H
#define WORLD_SIM_DIPLOMACY_BORDERS_H

void diplomacy_borders_reset(void);
void diplomacy_borders_mark_dirty(void);
int diplomacy_borders_dirty(void);
void diplomacy_borders_ensure(void);
void diplomacy_borders_clear_civ(int civ_id);
int diplomacy_pair_contact_stats(int civ_a, int civ_b, int *border_length, int *natural_barrier);

#endif
