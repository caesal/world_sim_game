#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/alliance_names.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/vassal.h"
#include "sim/world_announcement.h"

#include <stdio.h>
#include <string.h>

static AllianceSaveState alliance_state;

static int valid_alive(int civ_id) { return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive; }
static int sovereign(int civ_id) { return valid_alive(civ_id) && vassal_overlord(civ_id) < 0; }

static void clear_record(AllianceRecord *record, int id) {
    int i;
    memset(record, 0, sizeof(*record));
    record->id = id;
    record->founder_civ_id = -1;
    record->base_name_index = -1;
    for (i = 0; i < MAX_CIVS; i++) {
        record->members[i] = -1;
        record->joined_year_by_civ[i] = -1;
        alliance_state.join_years[i][id] = 0;
        alliance_state.kick_years[id][i] = 0;
        alliance_state.voluntary_cooldown[id][i] = 0;
        alliance_state.kicked_cooldown[id][i] = 0;
        alliance_state.council_vote_units[id][i] = 0;
        alliance_state.council_previous_vote_units[id][i] = 0;
        alliance_state.council_population_permille[id][i] = 0;
        alliance_state.council_province_permille[id][i] = 0;
    }
    alliance_state.alliance_type[id] = ALLIANCE_TYPE_DEFENSIVE;
    alliance_state.council_previous_valid[id] = 0;
    alliance_state.council_last_election_year[id] = 0;
    alliance_state.council_next_election_year[id] = 0;
    alliance_state.military_upgrade_cooldown[id] = 0;
    alliance_state.military_upgrade_active[id] = 0;
    alliance_state.military_upgrade_start_year[id] = 0;
    alliance_records_clear(id);
}

static void reset_membership(void) { int i; for (i = 0; i < MAX_CIVS; i++) alliance_state.civ_alliance[i] = -1; }

void alliance_reset(void) {
    int i;
    memset(&alliance_state, 0, sizeof(alliance_state));
    reset_membership();
    for (i = 0; i < ALLIANCE_MAX; i++) clear_record(&alliance_state.records[i], i);
    alliance_power_cache_reset();
}

static int active_alliance(int alliance_id) { return alliance_id >= 0 && alliance_id < alliance_state.next_id && alliance_state.records[alliance_id].active; }
static void mark_changed(void) { dirty_mark_diplomacy(); dirty_mark_alliance(); alliance_power_cache_reset(); }
static void reset_pair_score(int civ_a, int civ_b) { diplomacy_relation_score_reset_pair(civ_a, civ_b); }
static void set_pair_forced_cooldown(int civ_a, int civ_b, int years) {
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    alliance_state.create_years[civ_a][civ_b] = alliance_state.create_years[civ_b][civ_a] = years > 0 ? -years : 0;
}

static DiplomacyRelation ensure_known_relation(int civ_a, int civ_b, int fallback_score) {
    DiplomacyRelation relation = diplomacy_relation(civ_a, civ_b);
    if (relation.state == DIPLOMACY_NONE) {
        memset(&relation, 0, sizeof(relation));
        relation.state = DIPLOMACY_PEACE;
        relation.relation_score = fallback_score;
        relation.years_known = 1;
        relation.overlord = -1;
        relation.vassal = -1;
        relation.last_war_winner = -1;
        relation.last_war_loser = -1;
        relation.last_war_result = DIP_LAST_WAR_NONE;
    }
    return relation;
}

static int set_pair_alliance(int civ_a, int civ_b, int min_score) {
    DiplomacyRelation old_ab = diplomacy_relation(civ_a, civ_b);
    DiplomacyRelation old_ba = diplomacy_relation(civ_b, civ_a);
    DiplomacyRelation ab = ensure_known_relation(civ_a, civ_b, min_score);
    DiplomacyRelation ba = ensure_known_relation(civ_b, civ_a, min_score);
    ab.state = ba.state = DIPLOMACY_ALLIANCE;
    ab.truce_years_left = ba.truce_years_left = 0;
    ab.truce_initial_years = ba.truce_initial_years = 0;
    ab.overlord = ba.overlord = -1;
    ab.vassal = ba.vassal = -1;
    if (ab.relation_score < min_score) ab.relation_score = min_score;
    if (ba.relation_score < min_score) ba.relation_score = min_score;
    if (memcmp(&old_ab, &ab, sizeof(ab)) == 0 && memcmp(&old_ba, &ba, sizeof(ba)) == 0) return 0;
    diplomacy_restore_relation(civ_a, civ_b, ab);
    diplomacy_restore_relation(civ_b, civ_a, ba);
    reset_pair_score(civ_a, civ_b);
    return 1;
}

static int set_pair_peace_if_alliance(int civ_a, int civ_b) {
    DiplomacyRelation ab = diplomacy_relation(civ_a, civ_b);
    DiplomacyRelation ba = diplomacy_relation(civ_b, civ_a);
    if (ab.state != DIPLOMACY_ALLIANCE && ba.state != DIPLOMACY_ALLIANCE) return 0;
    if (ab.state == DIPLOMACY_ALLIANCE) ab.state = DIPLOMACY_PEACE;
    if (ba.state == DIPLOMACY_ALLIANCE) ba.state = DIPLOMACY_PEACE;
    ab.truce_years_left = ba.truce_years_left = 0;
    ab.truce_initial_years = ba.truce_initial_years = 0;
    ab.overlord = ba.overlord = -1;
    ab.vassal = ba.vassal = -1;
    diplomacy_restore_relation(civ_a, civ_b, ab);
    diplomacy_restore_relation(civ_b, civ_a, ba);
    reset_pair_score(civ_a, civ_b);
    return 1;
}

static int allocate_alliance(void) {
    int id;
    for (id = 0; id < alliance_state.next_id && id < ALLIANCE_MAX; id++) {
        if (!alliance_state.records[id].active) { clear_record(&alliance_state.records[id], id); return id; }
    }
    if (alliance_state.next_id >= ALLIANCE_MAX) return -1;
    id = alliance_state.next_id++;
    clear_record(&alliance_state.records[id], id);
    return id;
}

static void assign_name(AllianceRecord *record) {
    int count = alliance_name_base_count();
    int base = 0, i, best_count;
    if (count > 256) count = 256;
    for (i = 0; i < count; i++) {
        if (alliance_state.name_use_count[i] == 0) { base = i; break; }
    }
    if (i >= count && count > 0) {
        best_count = alliance_state.name_use_count[0];
        for (i = 1; i < count; i++) if (alliance_state.name_use_count[i] < best_count) { best_count = alliance_state.name_use_count[i]; base = i; }
    }
    if (base < 0 || base >= 256) base = 0;
    record->base_name_index = base;
    record->suffix_number = ++alliance_state.name_use_count[base];
    alliance_format_name(base, record->suffix_number, record->name_en, sizeof(record->name_en),
                         record->name_zh, sizeof(record->name_zh));
}

static void add_member_to_record(AllianceRecord *record, int civ_id) {
    if (!record || !sovereign(civ_id) || record->member_count >= MAX_CIVS) return;
    if (alliance_state.civ_alliance[civ_id] >= 0) return;
    record->members[record->member_count++] = civ_id;
    record->joined_year_by_civ[civ_id] = year;
    alliance_state.civ_alliance[civ_id] = record->id;
}

static int sync_record_pairs(const AllianceRecord *record, int min_score) {
    int i, j, changed = 0;
    if (!record || !record->active) return 0;
    for (i = 0; i < record->member_count; i++) {
        int a = record->members[i];
        if (!sovereign(a)) continue;
        for (j = i + 1; j < record->member_count; j++) {
            int b = record->members[j];
            if (sovereign(b)) changed |= set_pair_alliance(a, b, min_score);
        }
    }
    return changed;
}

static int create_alliance_internal(int founder, int second, int min_score) {
    AllianceRecord *record;
    int id;
    if (!sovereign(founder) || !sovereign(second) || founder == second) return -1;
    if (alliance_for_civ(founder) >= 0 || alliance_for_civ(second) >= 0) return -1;
    id = allocate_alliance();
    if (id < 0) return -1;
    record = &alliance_state.records[id];
    record->active = 1;
    record->founder_civ_id = founder;
    record->founded_year = year;
    record->color = civs[founder].color;
    assign_name(record);
    add_member_to_record(record, founder);
    add_member_to_record(record, second);
    sync_record_pairs(record, min_score);
    alliance_record_history(id, ALLIANCE_HISTORY_CREATED, founder, second,
                            ALLIANCE_VOTE_CREATE, ALLIANCE_REJECT_NONE);
    alliance_council_recalculate(id, 0);
    world_announcement_emit_alliance_created(id, founder, second);
    alliance_state.create_years[founder][second] = alliance_state.create_years[second][founder] = 0;
    mark_changed();
    return id;
}

static void disband_alliance(int alliance_id, int actor, int emit_event) {
    AllianceRecord *record;
    int members[MAX_CIVS];
    int count, i, j;
    if (!active_alliance(alliance_id)) return;
    record = &alliance_state.records[alliance_id];
    count = record->member_count;
    memcpy(members, record->members, sizeof(members));
    for (i = 0; i < count; i++) {
        if (members[i] >= 0 && members[i] < MAX_CIVS) alliance_state.civ_alliance[members[i]] = -1;
        for (j = i + 1; j < count; j++) set_pair_peace_if_alliance(members[i], members[j]);
    }
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_DISSOLVED, actor, -1,
                            -1, ALLIANCE_REJECT_ALLIANCE_DISSOLVED);
    if (emit_event) world_announcement_emit_alliance_dissolved(alliance_id, actor);
    record->active = 0;
    record->member_count = 0;
    mark_changed();
}

static void remove_member_from_record(AllianceRecord *record, int civ_id) {
    int i, out = 0;
    if (!record) return;
    for (i = 0; i < record->member_count; i++) {
        int member = record->members[i];
        if (member == civ_id) continue;
        record->members[out++] = member;
    }
    for (i = out; i < MAX_CIVS; i++) record->members[i] = -1;
    record->member_count = out;
    record->joined_year_by_civ[civ_id] = -1;
    alliance_state.civ_alliance[civ_id] = -1;
}

static void refresh_founder(AllianceRecord *record) {
    if (!record || record->member_count <= 0) return;
    if (!alliance_is_formal_member(record->id, record->founder_civ_id)) {
        record->founder_civ_id = record->members[0];
    }
    if (valid_alive(record->founder_civ_id)) record->color = civs[record->founder_civ_id].color;
}

static void leave_member(int alliance_id, int civ_id, int cooldown_years, int kicked, int emit_event) {
    AllianceRecord *record;
    int i, old_founder, hist_type;
    if (!active_alliance(alliance_id)) return;
    record = &alliance_state.records[alliance_id];
    old_founder = record->founder_civ_id;
    for (i = 0; i < record->member_count; i++) {
        int member = record->members[i];
        if (member != civ_id) set_pair_peace_if_alliance(civ_id, member);
    }
    if (cooldown_years > 0) {
        if (kicked) alliance_state.kicked_cooldown[alliance_id][civ_id] = cooldown_years;
        else alliance_state.voluntary_cooldown[alliance_id][civ_id] = cooldown_years;
    }
    remove_member_from_record(record, civ_id);
    hist_type = kicked == 2 ? ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT :
                (kicked ? ALLIANCE_HISTORY_MEMBER_REMOVED : ALLIANCE_HISTORY_MEMBER_LEFT);
    alliance_record_history(alliance_id, hist_type, civ_id, -1,
                            kicked ? ALLIANCE_VOTE_REMOVAL : -1, ALLIANCE_REJECT_NONE);
    if (record->member_count < 2) {
        disband_alliance(alliance_id, civ_id, emit_event);
        return;
    }
    refresh_founder(record);
    if (record->founder_civ_id != old_founder) {
        alliance_military_downgrade(alliance_id, ALLIANCE_HISTORY_DOWNGRADED_LEADER_TRANSFERRED);
        alliance_record_history(alliance_id, ALLIANCE_HISTORY_LEADER_CHANGED,
                                record->founder_civ_id, old_founder, -1,
                                ALLIANCE_REJECT_NONE);
    }
    sync_record_pairs(record, 0);
    alliance_council_recalculate(alliance_id, 1);
    if (emit_event) {
        world_announcement_emit_alliance_member_removed(
            alliance_id, civ_id, record->founder_civ_id);
    }
    mark_changed();
}

static void clear_player_pair_blocks(int civ_a, int civ_b) {
    if (civ_a >= 0 && civ_a < MAX_CIVS && civ_b >= 0 && civ_b < MAX_CIVS) {
        alliance_state.create_years[civ_a][civ_b] = alliance_state.create_years[civ_b][civ_a] = 0;
    }
}

static void clear_player_join_blocks(int alliance_id, int civ_id) {
    int i;
    if (!active_alliance(alliance_id) || civ_id < 0 || civ_id >= MAX_CIVS) return;
    alliance_state.join_years[civ_id][alliance_id] = 0;
    alliance_state.kick_years[alliance_id][civ_id] = 0;
    alliance_state.voluntary_cooldown[alliance_id][civ_id] = 0;
    alliance_state.kicked_cooldown[alliance_id][civ_id] = 0;
    for (i = 0; i < alliance_state.records[alliance_id].member_count; i++)
        clear_player_pair_blocks(civ_id, alliance_state.records[alliance_id].members[i]);
}

AllianceCommandResult alliance_player_form_or_join(int source_civ, int target_civ) {
    int source_alliance, target_alliance;
    if (!valid_alive(source_civ)) return ALLIANCE_CMD_INVALID_SOURCE;
    if (!valid_alive(target_civ)) return ALLIANCE_CMD_INVALID_TARGET;
    if (source_civ == target_civ) return ALLIANCE_CMD_SELF_TARGET;
    if (vassal_overlord(source_civ) >= 0) return ALLIANCE_CMD_SOURCE_VASSAL;
    if (vassal_overlord(target_civ) >= 0) return ALLIANCE_CMD_TARGET_VASSAL;
    source_alliance = alliance_for_civ(source_civ);
    target_alliance = alliance_for_civ(target_civ);
    if (source_alliance >= 0 && target_alliance >= 0) {
        return source_alliance == target_alliance ? ALLIANCE_CMD_ALREADY_SAME :
                                                    ALLIANCE_CMD_DIFFERENT_ALLIANCES;
    }
    if (source_alliance < 0 && target_alliance < 0) {
        clear_player_pair_blocks(source_civ, target_civ);
        return create_alliance_internal(source_civ, target_civ, 80) >= 0 ?
               ALLIANCE_CMD_OK : ALLIANCE_CMD_NO_SLOT;
    }
    if (source_alliance < 0) {
        clear_player_join_blocks(target_alliance, source_civ);
        return alliance_debug_add_member(target_alliance, source_civ, 80) ?
                               ALLIANCE_CMD_OK : ALLIANCE_CMD_BLOCKED;
    }
    clear_player_join_blocks(source_alliance, target_civ);
    return alliance_debug_add_member(source_alliance, target_civ, 80) ? ALLIANCE_CMD_OK : ALLIANCE_CMD_BLOCKED;
}

AllianceCommandResult alliance_player_leave(int source_civ) {
    int alliance_id;
    if (!valid_alive(source_civ)) return ALLIANCE_CMD_INVALID_SOURCE;
    if (vassal_overlord(source_civ) >= 0) return ALLIANCE_CMD_SOURCE_VASSAL;
    alliance_id = alliance_for_civ(source_civ);
    if (!active_alliance(alliance_id)) return ALLIANCE_CMD_NO_ALLIANCE;
    if (alliance_member_count(alliance_id) <= 2) disband_alliance(alliance_id, source_civ, 1);
    else leave_member(alliance_id, source_civ, 10, 0, 1);
    return ALLIANCE_CMD_OK;
}

int alliance_break_for_player_war(int civ_a, int civ_b) {
    int alliance_id = alliance_display_for_civ(civ_a);
    if (alliance_id < 0 || alliance_id != alliance_display_for_civ(civ_b)) return 0;
    if (alliance_for_civ(civ_a) == alliance_id) leave_member(alliance_id, civ_a, 0, 0, 1);
    else if (alliance_for_civ(civ_b) == alliance_id) leave_member(alliance_id, civ_b, 0, 0, 1);
    else set_pair_peace_if_alliance(civ_a, civ_b);
    return 1;
}

int alliance_force_member_exit_for_war_defeat(int alliance_id, int civ_id, int cooldown_years) {
    AllianceRecord *record;
    int members[MAX_CIVS], count, i;
    if (!active_alliance(alliance_id) || !alliance_is_formal_member(alliance_id, civ_id)) return 0;
    record = &alliance_state.records[alliance_id];
    count = record->member_count;
    memcpy(members, record->members, sizeof(members));
    for (i = 0; i < count; i++) if (members[i] != civ_id) set_pair_forced_cooldown(civ_id, members[i], cooldown_years);
    leave_member(alliance_id, civ_id, cooldown_years, 2, 0);
    return 1;
}

int alliance_debug_create_pair(int founder_civ, int second_civ, int min_score) {
    return create_alliance_internal(founder_civ, second_civ, min_score);
}

static int add_member_direct(int alliance_id, int civ_id, int min_score, int record_candidate) {
    AllianceRecord *record;
    if (!active_alliance(alliance_id) || !sovereign(civ_id) || alliance_for_civ(civ_id) >= 0) return 0;
    record = &alliance_state.records[alliance_id];
    add_member_to_record(record, civ_id);
    sync_record_pairs(record, min_score);
    alliance_council_recalculate(alliance_id, 1);
    if (record_candidate)
        alliance_record_candidate(alliance_id, civ_id, ALLIANCE_CANDIDATE_JOIN,
                                  ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE, year, 100,
                                  ALLIANCE_CANDIDATE_PASSED, ALLIANCE_REJECT_NONE);
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_MEMBER_JOINED,
                            civ_id, -1, ALLIANCE_VOTE_JOIN, ALLIANCE_REJECT_NONE);
    world_announcement_emit_alliance_member_joined(
        alliance_id, record->founder_civ_id, civ_id);
    mark_changed();
    return 1;
}

int alliance_debug_add_member(int alliance_id, int civ_id, int min_score) {
    return add_member_direct(alliance_id, civ_id, min_score, 1);
}

AllianceCommandResult alliance_player_invite_member(int alliance_id, int target_civ) {
    int current;
    if (!active_alliance(alliance_id)) return ALLIANCE_CMD_NO_ALLIANCE;
    if (!valid_alive(target_civ)) return ALLIANCE_CMD_INVALID_TARGET;
    if (vassal_overlord(target_civ) >= 0) return ALLIANCE_CMD_TARGET_VASSAL;
    current = alliance_for_civ(target_civ);
    if (current >= 0) return current == alliance_id ? ALLIANCE_CMD_ALREADY_SAME :
                                                      ALLIANCE_CMD_DIFFERENT_ALLIANCES;
    clear_player_join_blocks(alliance_id, target_civ);
    return add_member_direct(alliance_id, target_civ, 80, 0) ? ALLIANCE_CMD_OK :
                                                               ALLIANCE_CMD_BLOCKED;
}

AllianceSaveState *alliance_internal_state(void) { return &alliance_state; }

int alliance_debug_kick_member(int alliance_id, int civ_id, int cooldown_years) {
    if (!active_alliance(alliance_id) || !alliance_is_formal_member(alliance_id, civ_id)) return 0;
    leave_member(alliance_id, civ_id, cooldown_years, 1, 1);
    return 1;
}

AllianceCommandResult alliance_player_remove_member(int alliance_id, int target_civ) {
    if (!active_alliance(alliance_id)) return ALLIANCE_CMD_NO_ALLIANCE;
    if (!valid_alive(target_civ)) return ALLIANCE_CMD_INVALID_TARGET;
    if (!alliance_is_formal_member(alliance_id, target_civ)) return ALLIANCE_CMD_NO_ALLIANCE;
    leave_member(alliance_id, target_civ, 100, 1, 1);
    return ALLIANCE_CMD_OK;
}

void alliance_sanitize_loaded(void) {
    int a, b, id;
    int changed = 0;
    for (a = 0; a < MAX_CIVS; a++) alliance_state.civ_alliance[a] = -1;
    for (id = 0; id < alliance_state.next_id && id < ALLIANCE_MAX; id++) {
        AllianceRecord *record = &alliance_state.records[id];
        int out = 0, i;
        if (!record->active) continue;
        if (alliance_state.alliance_type[id] != ALLIANCE_TYPE_MILITARY)
            alliance_state.alliance_type[id] = ALLIANCE_TYPE_DEFENSIVE;
        for (i = 0; i < record->member_count; i++) {
            int civ_id = record->members[i];
            if (!sovereign(civ_id) || alliance_state.civ_alliance[civ_id] >= 0) {
                changed = 1;
                continue;
            }
            record->members[out++] = civ_id;
            alliance_state.civ_alliance[civ_id] = id;
        }
        if (record->member_count != out) changed = 1;
        record->member_count = out;
        if (out < 2) {
            record->active = 0;
            changed = 1;
        } else {
            if (!valid_alive(record->founder_civ_id)) {
                alliance_military_downgrade(id, ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL);
            }
            refresh_founder(record);
            changed |= sync_record_pairs(record, 0);
            if (alliance_state.council_next_election_year[id] <= 0)
                alliance_council_recalculate(id, 0);
        }
    }
    for (a = 0; a < civ_count && a < MAX_CIVS; a++) {
        for (b = a + 1; b < civ_count && b < MAX_CIVS; b++) {
            if (alliance_for_civ(a) != alliance_for_civ(b) || alliance_for_civ(a) < 0)
                changed |= set_pair_peace_if_alliance(a, b);
        }
    }
    if (changed) mark_changed();
}
