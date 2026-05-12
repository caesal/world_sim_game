#ifndef WORLD_SIM_WAR_FRONT_H
#define WORLD_SIM_WAR_FRONT_H

typedef enum {
    WAR_FRONT_NONE = 0,
    WAR_FRONT_LAND = 1,
    WAR_FRONT_SEA = 2,
    WAR_FRONT_VASSAL_LAND = 4,
    WAR_FRONT_VASSAL_SEA = 8
} WarFrontFlags;

int war_front_control_group(int civ_id, int *out_ids, int max_ids);
int war_front_flags(int civ_a, int civ_b);
int war_has_active_front(int civ_a, int civ_b);
const char *war_front_reason_en(int flags);
const char *war_front_reason_zh(int flags);

#endif
