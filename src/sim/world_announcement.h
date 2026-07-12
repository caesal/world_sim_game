#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_H

void world_announcement_state_reset(void);
void world_announcement_state_baseline_from_world(void);

void world_announcement_emit_age_first(int civ_id, int stage);
void world_announcement_emit_deep_sea_first(int civ_id);
void world_announcement_emit_collapse(int civ_id, int target_id, int region_id,
                                      const int *successor_ids, int successor_count,
                                      int event_param);
void world_announcement_emit_vassal(int event_type, int vassal_id,
                                    int overlord_id, int other_overlord_id,
                                    int event_param);
void world_announcement_emit_vassal_detail(int event_type, int vassal_id,
                                           int overlord_id, int other_overlord_id,
                                           int region_id, int city_id,
                                           int param_a, int param_b);
void world_announcement_plague_observe(void);

void world_announcement_emit_alliance_created(int alliance_id, int founder, int second);
void world_announcement_emit_alliance_dissolved(int alliance_id, int actor);
void world_announcement_emit_alliance_member_joined(int alliance_id, int founder, int member);
void world_announcement_emit_alliance_member_removed(int alliance_id, int member, int founder);
void world_announcement_emit_alliance_military_changed(int alliance_id, int upgraded);
void world_announcement_emit_union(int alliance_id, int proposer,
                                   const int *members, int member_count);

void world_announcement_war_reset(void);
void world_announcement_war_baseline_active(void);
void world_announcement_war_started(int slot, int attacker, int defender);
void world_announcement_war_ended(int slot, int outcome, int last_war_result);

#endif
