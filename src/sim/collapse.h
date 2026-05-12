#ifndef WORLD_SIM_COLLAPSE_H
#define WORLD_SIM_COLLAPSE_H

typedef enum {
    COLLAPSE_CAUSE_PRESSURE,
    COLLAPSE_CAUSE_CIVIL_UNREST
} CollapseCause;

typedef enum {
    COLLAPSE_BLOCK_NONE,
    COLLAPSE_BLOCK_NOT_ALIVE,
    COLLAPSE_BLOCK_MAX_CIVS,
    COLLAPSE_BLOCK_NO_CAPITAL_REGION,
    COLLAPSE_BLOCK_NO_SPLITTABLE_REGION,
    COLLAPSE_BLOCK_ONLY_CORE_LEFT,
    COLLAPSE_BLOCK_CITY_CAP,
    COLLAPSE_BLOCK_UNKNOWN
} CollapseBlockReason;

int collapse_decade_chance_for_disorder(int disorder);
int collapse_probability_for_disorder(int disorder);
CollapseBlockReason collapse_block_reason(int civ_id);
const char *collapse_block_reason_text(CollapseBlockReason reason);
int collapse_can_trigger(int civ_id);
const char *collapse_trigger_block_reason(int civ_id);
const char *collapse_last_reason(int civ_id);
int collapse_trigger(int civ_id, CollapseCause cause);
int collapse_check_immediate(int civ_id, CollapseCause cause);
void collapse_update_immediate(void);
void collapse_update_decade(void);
int collapse_grace_months_left(int civ_id);

#endif
