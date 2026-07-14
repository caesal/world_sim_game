#include "sim/alliance.h"
#include "sim/alliance_military.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/population_military.h"
#include "sim/war_internal.h"
#include "sim/world_announcement.h"

#include <string.h>

static int active_alliance(const AllianceSaveState *state, int alliance_id) {
    return state && alliance_id >= 0 && alliance_id < state->next_id &&
           alliance_id < ALLIANCE_MAX && state->records[alliance_id].active;
}

static int sovereign_alive(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive;
}

static int vote_roll(int chance_percent) {
    return rnd(100) < clamp(chance_percent, 0, 100);
}

int alliance_military_eligible(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceRecord *record;
    int i, j;
    if (!active_alliance(state, alliance_id)) return 0;
    record = &state->records[alliance_id];
    if (state->alliance_type[alliance_id] != ALLIANCE_TYPE_DEFENSIVE) return 0;
    if (year - record->founded_year < ALLIANCE_MILITARY_ELIGIBLE_YEARS) return 0;
    if (record->member_count < 2) return 0;
    for (i = 0; i < record->member_count; i++) {
        int a = record->members[i];
        if (!sovereign_alive(a)) return 0;
        for (j = 0; j < record->member_count; j++) {
            int b = record->members[j];
            if (i == j) continue;
            if (diplomacy_relation(a, b).relation_score < 95) return 0;
        }
    }
    return 1;
}

static void record_upgrade_candidate(AllianceSaveState *state, int alliance_id, int status, int reason) {
    int leader = state->records[alliance_id].founder_civ_id;
    alliance_record_candidate(alliance_id, leader, ALLIANCE_CANDIDATE_MILITARY_UPGRADE,
                              ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE,
                              state->military_upgrade_start_year[alliance_id] > 0 ?
                              state->military_upgrade_start_year[alliance_id] : year,
                              status == ALLIANCE_CANDIDATE_ACTIVE ? 50 : 100, status, reason);
}

static int member_votes_yes(const AllianceRecord *record, int member) {
    int i;
    if (!record || !sovereign_alive(member)) return 0;
    for (i = 0; i < record->member_count; i++) {
        int other = record->members[i];
        if (other == member) continue;
        if (diplomacy_relation(member, other).relation_score < 95) return 0;
    }
    return 1;
}

static void resolve_upgrade_vote(AllianceSaveState *state, int alliance_id) {
    AllianceRecord *record = &state->records[alliance_id];
    signed char votes[MAX_CIVS];
    int council_units[MAX_CIVS];
    int yes_units = 0, no_units = 0, i, passed;
    memset(votes, ALLIANCE_MEMBER_VOTE_NA, sizeof(votes));
    memset(council_units, 0, sizeof(council_units));
    for (i = 0; i < record->member_count; i++) {
        int member = record->members[i];
        int units = state->council_vote_units[alliance_id][member];
        if (member >= 0 && member < MAX_CIVS) council_units[member] = units;
        if (member_votes_yes(record, member) && vote_roll(ALLIANCE_MILITARY_UPGRADE_YES_CHANCE)) {
            votes[member] = ALLIANCE_MEMBER_VOTE_YES;
            yes_units += units;
        } else {
            votes[member] = ALLIANCE_MEMBER_VOTE_NO;
            no_units += units;
        }
    }
    passed = yes_units * 3 > ALLIANCE_COUNCIL_TOTAL_UNITS * 2;
    alliance_record_weighted_vote(alliance_id, ALLIANCE_VOTE_MILITARY_UPGRADE,
                                  record->founder_civ_id, -1, votes, council_units,
                                  yes_units, no_units, passed,
                                  passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_history(alliance_id, passed ? ALLIANCE_HISTORY_MILITARY_UPGRADE_PASSED :
                            ALLIANCE_HISTORY_MILITARY_UPGRADE_FAILED,
                            record->founder_civ_id, -1, ALLIANCE_VOTE_MILITARY_UPGRADE,
                            passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    record_upgrade_candidate(state, alliance_id, passed ? ALLIANCE_CANDIDATE_PASSED :
                             ALLIANCE_CANDIDATE_REJECTED,
                             passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    state->military_upgrade_active[alliance_id] = 0;
    state->military_upgrade_start_year[alliance_id] = 0;
    state->military_upgrade_cooldown[alliance_id] = ALLIANCE_MILITARY_RETRY_YEARS;
    if (passed) {
        state->alliance_type[alliance_id] = ALLIANCE_TYPE_MILITARY;
        alliance_record_history(alliance_id, ALLIANCE_HISTORY_UPGRADED_TO_MILITARY,
                                record->founder_civ_id, -1, ALLIANCE_VOTE_MILITARY_UPGRADE,
                                ALLIANCE_REJECT_NONE);
        world_announcement_emit_alliance_military_changed(alliance_id, 1);
    }
    dirty_mark_alliance();
}

void alliance_military_downgrade(int alliance_id, int history_type) {
    AllianceSaveState *state = alliance_internal_state();
    if (!active_alliance(state, alliance_id) ||
        state->alliance_type[alliance_id] != ALLIANCE_TYPE_MILITARY) return;
    state->alliance_type[alliance_id] = ALLIANCE_TYPE_DEFENSIVE;
    state->military_upgrade_active[alliance_id] = 0;
    state->military_upgrade_start_year[alliance_id] = 0;
    state->military_upgrade_cooldown[alliance_id] = ALLIANCE_MILITARY_RETRY_YEARS;
    alliance_record_history(alliance_id, history_type, state->records[alliance_id].founder_civ_id,
                            -1, ALLIANCE_VOTE_MILITARY_UPGRADE, ALLIANCE_REJECT_NONE);
    world_announcement_emit_alliance_military_changed(alliance_id, 0);
    dirty_mark_alliance();
}

int alliance_military_update_year_step(AllianceYearWork *work) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || !work) return 0;
    while (work->alliance_id < state->next_id && work->alliance_id < ALLIANCE_MAX) {
        int id = work->alliance_id++;
        AllianceRecord *record = &state->records[id];
        if (!record->active) continue;
        if (state->military_upgrade_cooldown[id] > 0) state->military_upgrade_cooldown[id]--;
        if (state->alliance_type[id] == ALLIANCE_TYPE_MILITARY) {
            if (!sovereign_alive(record->founder_civ_id))
                alliance_military_downgrade(id, ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL);
            return 1;
        }
        if (state->military_upgrade_active[id]) {
            if (year > state->military_upgrade_start_year[id]) resolve_upgrade_vote(state, id);
            return 1;
        }
        if (state->military_upgrade_cooldown[id] <= 0 && alliance_military_eligible(id)) {
            state->military_upgrade_active[id] = 1;
            state->military_upgrade_start_year[id] = year;
            record_upgrade_candidate(state, id, ALLIANCE_CANDIDATE_ACTIVE, ALLIANCE_REJECT_NONE);
            alliance_record_history(id, ALLIANCE_HISTORY_MILITARY_UPGRADE_INITIATED,
                                    record->founder_civ_id, -1, ALLIANCE_VOTE_MILITARY_UPGRADE,
                                    ALLIANCE_REJECT_NONE);
        }
        return 1;
    }
    return 0;
}

static int supported_war_count(int contributor, int alliance_id) {
    int i, count = 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];
        if (!war->active || war->attacker == contributor || war->defender == contributor) continue;
        if (alliance_for_civ(war->attacker) == alliance_id ||
            alliance_for_civ(war->defender) == alliance_id) count++;
    }
    return max(1, count);
}

int alliance_military_support_for_war(const ActiveWar *war, int attacker_side) {
    int primary;
    int alliance_id;
    int i, total = 0;
    if (!war) return 0;
    primary = attacker_side ? war->attacker : war->defender;
    alliance_id = alliance_for_civ(primary);
    if (alliance_type(alliance_id) != ALLIANCE_TYPE_MILITARY) return 0;
    for (i = 0; i < alliance_member_count(alliance_id); i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        int soldiers;
        if (member == primary || member == war->attacker || member == war->defender) continue;
        soldiers = population_military_current_soldiers_for_civ(member, support_casualties[member]);
        total += (soldiers * 45 / 100) / supported_war_count(member, alliance_id);
    }
    return total;
}

static void apply_population_losses(int civ_id, int losses) {
    int population_losses = population_apply_casualties(civ_id, losses);
    if (population_losses > 0) {
        disorder_add_war_deaths(civ_id, population_losses);
    }
}

int alliance_military_apply_support_casualties(const ActiveWar *war, int attacker_side) {
    int primary;
    int alliance_id;
    int i, total = 0;
    if (!war) return 0;
    primary = attacker_side ? war->attacker : war->defender;
    alliance_id = alliance_for_civ(primary);
    if (alliance_type(alliance_id) != ALLIANCE_TYPE_MILITARY) return 0;
    for (i = 0; i < alliance_member_count(alliance_id); i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        int support, losses;
        if (member == primary || member == war->attacker || member == war->defender) continue;
        support = (population_military_current_soldiers_for_civ(member, support_casualties[member]) *
                   45 / 100) / supported_war_count(member, alliance_id);
        losses = support * 15 / 1000;
        if (support > 0 && losses <= 0) losses = 1;
        support_casualties[member] += losses;
        apply_population_losses(member, losses);
        total += losses;
    }
    return total;
}
