#ifndef WORLD_SIM_CIV_COLORS_H
#define WORLD_SIM_CIV_COLORS_H

#include "core/value_types.h"

Color32 civilization_pick_auto_color(int civ_id, int seed_region);
Color32 civilization_pick_distinct_color(int civ_id, Color32 preferred_color,
                                         int parent_civ_id, int seed_region);
Color32 civilization_pick_distinct_color_for_regions(int civ_id, Color32 preferred_color,
                                                     int parent_civ_id, int seed_region,
                                                     const int *regions, int region_count);
Color32 civilization_preview_distinct_color(int civ_id, Color32 preferred_color,
                                            int parent_civ_id, int seed_region);
int civilization_colors_too_similar_for_display(Color32 a, Color32 b);
int civilization_color_display_distance(Color32 a, Color32 b);
void civilization_color_reset_manual_locks(void);
void civilization_color_mark_manual(int civ_id);
int civilization_color_manual_locked(int civ_id);
void civilization_color_note_region_claim(int owner, int region_id);
int civilization_repair_queued_color_conflicts(int budget);
void civilization_color_repair_note_changed(void);
void civilization_color_repair_note_unresolved(void);
int civilization_color_repair_changed_count(void);
int civilization_color_repair_unresolved_count(void);
int civilization_color_repair_cooldown_count(void);
int civilization_colors_debug_check(void);
int civilization_repair_alive_colors(void);

#endif
