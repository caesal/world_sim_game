#ifndef WORLD_SIM_ALLIANCE_H
#define WORLD_SIM_ALLIANCE_H

#include "core/constants.h"
#include "core/value_types.h"

#define ALLIANCE_MAX MAX_CIVS
#define ALLIANCE_NAME_LEN 96

typedef enum {
    ALLIANCE_CMD_OK = 0,
    ALLIANCE_CMD_INVALID_SOURCE,
    ALLIANCE_CMD_INVALID_TARGET,
    ALLIANCE_CMD_SELF_TARGET,
    ALLIANCE_CMD_SOURCE_VASSAL,
    ALLIANCE_CMD_TARGET_VASSAL,
    ALLIANCE_CMD_ALREADY_SAME,
    ALLIANCE_CMD_DIFFERENT_ALLIANCES,
    ALLIANCE_CMD_NO_ALLIANCE,
    ALLIANCE_CMD_BLOCKED,
    ALLIANCE_CMD_NO_SLOT
} AllianceCommandResult;

typedef struct {
    int active;
    int id;
    int founder_civ_id;
    int founded_year;
    int base_name_index;
    int suffix_number;
    int member_count;
    Color32 color;
    int members[MAX_CIVS];
    int joined_year_by_civ[MAX_CIVS];
    char name_en[ALLIANCE_NAME_LEN];
    char name_zh[ALLIANCE_NAME_LEN];
} AllianceRecord;

typedef struct {
    int active;
    int id;
    int founder_civ_id;
    int founded_year;
    int member_count;
    Color32 color;
    char name_en[ALLIANCE_NAME_LEN];
    char name_zh[ALLIANCE_NAME_LEN];
} AllianceSnapshotRecord;

typedef struct {
    AllianceRecord records[ALLIANCE_MAX];
    int civ_alliance[MAX_CIVS];
    int next_id;
    int name_use_count[256];
    int create_years[MAX_CIVS][MAX_CIVS];
    int join_years[MAX_CIVS][ALLIANCE_MAX];
    int kick_years[ALLIANCE_MAX][MAX_CIVS];
    int voluntary_cooldown[ALLIANCE_MAX][MAX_CIVS];
    int kicked_cooldown[ALLIANCE_MAX][MAX_CIVS];
} AllianceSaveState;

typedef struct {
    int phase;
    int row;
    int col;
    int civ_a;
    int civ_b;
    int candidate;
    int alliance_id;
    int index;
    int count;
} AllianceYearWork;

void alliance_reset(void);
void alliance_sanitize_loaded(void);
void alliance_update_year(void);
void alliance_year_work_begin(AllianceYearWork *work);
int alliance_update_year_step(AllianceYearWork *work, int work_budget);
int alliance_year_last_step_ms(void);
int alliance_year_peak_step_ms(void);

int alliance_for_civ(int civ_id);
int alliance_display_for_civ(int civ_id);
int alliance_member_count(int alliance_id);
int alliance_founder(int alliance_id);
int alliance_member_order(int alliance_id, int civ_id);
int alliance_formal_member_at(int alliance_id, int index);
int alliance_is_formal_member(int alliance_id, int civ_id);
Color32 alliance_color(int alliance_id);
const char *alliance_name_en(int alliance_id);
const char *alliance_name_zh(int alliance_id);

AllianceCommandResult alliance_player_form_or_join(int source_civ, int target_civ);
AllianceCommandResult alliance_player_leave(int source_civ);
int alliance_break_for_player_war(int civ_a, int civ_b);

int alliance_own_power(int civ_id);
int alliance_defensive_bloc_power(int civ_id);
void alliance_power_cache_reset(void);
int alliance_power_own_recompute_count(void);
int alliance_power_bloc_recompute_count(void);

int alliance_copy_snapshot_records(AllianceSnapshotRecord *out_records, int max_records);
void alliance_copy_save_state(AllianceSaveState *out_state);
void alliance_restore_save_state(const AllianceSaveState *state);

int alliance_debug_create_pair(int founder_civ, int second_civ, int min_score);
int alliance_debug_add_member(int alliance_id, int civ_id, int min_score);
int alliance_debug_kick_member(int alliance_id, int civ_id, int cooldown_years);
AllianceSaveState *alliance_internal_state(void);
void alliance_debug_set_create_years(int civ_a, int civ_b, int years);
void alliance_debug_set_join_years(int civ_id, int alliance_id, int years);
void alliance_debug_set_kick_years(int alliance_id, int civ_id, int years);
void alliance_debug_set_cooldown(int alliance_id, int civ_id, int voluntary, int years);

#endif
