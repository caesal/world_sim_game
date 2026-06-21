#ifndef WORLD_SIM_ALLIANCE_H
#define WORLD_SIM_ALLIANCE_H

#include "core/constants.h"
#include "core/value_types.h"

#define ALLIANCE_MAX MAX_CIVS
#define ALLIANCE_NAME_LEN 96
#define ALLIANCE_CANDIDATE_RECORD_CAP 32
#define ALLIANCE_VOTE_RECORD_CAP 32
#define ALLIANCE_HISTORY_RECORD_CAP 128

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

typedef enum {
    ALLIANCE_CANDIDATE_JOIN = 0,
    ALLIANCE_CANDIDATE_REMOVAL = 1
} AllianceCandidateType;

typedef enum {
    ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE = 0,
    ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE = 1,
    ALLIANCE_CANDIDATE_INITIATOR_SYSTEM = 2
} AllianceCandidateInitiator;

typedef enum {
    ALLIANCE_CANDIDATE_ACTIVE = 0,
    ALLIANCE_CANDIDATE_PASSED = 1,
    ALLIANCE_CANDIDATE_REJECTED = 2,
    ALLIANCE_CANDIDATE_BLOCKED = 3
} AllianceCandidateStatus;

typedef enum {
    ALLIANCE_VOTE_CREATE = 0,
    ALLIANCE_VOTE_JOIN = 1,
    ALLIANCE_VOTE_REMOVAL = 2
} AllianceVoteType;

typedef enum {
    ALLIANCE_MEMBER_VOTE_NA = -1,
    ALLIANCE_MEMBER_VOTE_ABSTAIN = 0,
    ALLIANCE_MEMBER_VOTE_YES = 1,
    ALLIANCE_MEMBER_VOTE_NO = 2
} AllianceMemberVote;

typedef enum {
    ALLIANCE_REJECT_NONE = 0,
    ALLIANCE_REJECT_VOTE_FAILED = 1,
    ALLIANCE_REJECT_RELATION_BELOW_THRESHOLD = 2,
    ALLIANCE_REJECT_COOLDOWN_ACTIVE = 3,
    ALLIANCE_REJECT_HARD_BLOCKER = 4,
    ALLIANCE_REJECT_BECAME_VASSAL = 5,
    ALLIANCE_REJECT_JOINED_ANOTHER_ALLIANCE = 6,
    ALLIANCE_REJECT_ALLIANCE_DISSOLVED = 7
} AllianceRejectionReason;

typedef enum {
    ALLIANCE_HISTORY_CREATED = 0,
    ALLIANCE_HISTORY_CANDIDATE_APPEARED = 1,
    ALLIANCE_HISTORY_VOTE_RESOLVED = 2,
    ALLIANCE_HISTORY_VOTE_PASSED = 3,
    ALLIANCE_HISTORY_VOTE_FAILED = 4,
    ALLIANCE_HISTORY_MEMBER_JOINED = 5,
    ALLIANCE_HISTORY_MEMBER_LEFT = 6,
    ALLIANCE_HISTORY_MEMBER_REMOVED = 7,
    ALLIANCE_HISTORY_LEADER_CHANGED = 8,
    ALLIANCE_HISTORY_DISSOLVED = 9
} AllianceHistoryType;

typedef struct {
    int active;
    int alliance_id;
    int civ_id;
    int type;
    int initiated_by;
    int candidate_year;
    int qualification_progress;
    int status;
    int rejection_reason;
    int updated_year;
} AllianceCandidateRecord;

typedef struct {
    int active;
    int alliance_id;
    int vote_year;
    int vote_type;
    int target_civ_id;
    int secondary_civ_id;
    int yes_count;
    int no_count;
    int passed;
    int rejection_reason;
    signed char member_votes[MAX_CIVS];
} AllianceVoteRecord;

typedef struct {
    int active;
    int alliance_id;
    int event_year;
    int event_type;
    int civ_id;
    int target_civ_id;
    int vote_type;
    int rejection_reason;
} AllianceHistoryRecord;

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
    int members[MAX_CIVS];
    int joined_year_by_civ[MAX_CIVS];
    int candidate_count;
    int candidate_next;
    int vote_count;
    int vote_next;
    int history_count;
    int history_next;
    AllianceCandidateRecord candidates[ALLIANCE_CANDIDATE_RECORD_CAP];
    AllianceVoteRecord votes[ALLIANCE_VOTE_RECORD_CAP];
    AllianceHistoryRecord history[ALLIANCE_HISTORY_RECORD_CAP];
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
    int candidate_count[ALLIANCE_MAX];
    int candidate_next[ALLIANCE_MAX];
    int vote_count[ALLIANCE_MAX];
    int vote_next[ALLIANCE_MAX];
    int history_count[ALLIANCE_MAX];
    int history_next[ALLIANCE_MAX];
    AllianceCandidateRecord candidates[ALLIANCE_MAX][ALLIANCE_CANDIDATE_RECORD_CAP];
    AllianceVoteRecord votes[ALLIANCE_MAX][ALLIANCE_VOTE_RECORD_CAP];
    AllianceHistoryRecord history[ALLIANCE_MAX][ALLIANCE_HISTORY_RECORD_CAP];
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
void alliance_records_clear(int alliance_id);
void alliance_record_candidate(int alliance_id, int civ_id, int type, int initiated_by,
                               int candidate_year, int progress, int status, int reason);
void alliance_record_vote(int alliance_id, int vote_type, int target_civ_id, int secondary_civ_id,
                          const signed char *member_votes, int yes_count, int no_count,
                          int passed, int reason);
void alliance_record_history(int alliance_id, int event_type, int civ_id, int target_civ_id,
                             int vote_type, int reason);
void alliance_debug_set_create_years(int civ_a, int civ_b, int years);
void alliance_debug_set_join_years(int civ_id, int alliance_id, int years);
void alliance_debug_set_kick_years(int alliance_id, int civ_id, int years);
void alliance_debug_set_cooldown(int alliance_id, int civ_id, int voluntary, int years);

#endif
