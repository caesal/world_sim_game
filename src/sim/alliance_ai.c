#include "sim/alliance.h"

#include "core/game_state.h"
#include "core/profiler.h"
#include "sim/diplomacy.h"
#include "sim/vassal.h"
#include "sim/war.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

enum {
    ALLIANCE_YEAR_DECAY_VOLUNTARY,
    ALLIANCE_YEAR_DECAY_KICKED,
    ALLIANCE_YEAR_PRE_SANITIZE,
    ALLIANCE_YEAR_CREATION,
    ALLIANCE_YEAR_JOIN,
    ALLIANCE_YEAR_KICK,
    ALLIANCE_YEAR_POST_SANITIZE,
    ALLIANCE_YEAR_DONE
};

static int alliance_year_last_ms = 0;
static int alliance_year_peak_ms = 0;

static int sovereign_alive(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS &&
           civs[civ_id].alive && vassal_overlord(civ_id) < 0;
}

static int no_hard_pair_block(int civ_a, int civ_b) {
    DiplomacyStatus state = diplomacy_status(civ_a, civ_b);
    return diplomacy_current_contact_kind(civ_a, civ_b) != DIP_CONTACT_NONE &&
           state != DIPLOMACY_WAR && state != DIPLOMACY_TRUCE &&
           state != DIPLOMACY_VASSAL && !war_active_between(civ_a, civ_b);
}

static int vote_roll(int chance_percent) {
    return rnd(100) < clamp(chance_percent, 0, 100);
}

static int create_chance(int score) {
    return clamp(45 + (score - 95) * 4, 45, 65);
}

static int join_chance(int score) {
    return clamp(10 + (score - 60) * 5 / 4, 10, 60);
}

static int kick_yes_chance(int score) {
    if (score >= 60) return 0;
    if (score < 0) return 100;
    if (score <= 30) return 100 - score * 45 / 30;
    return 55 - (score - 30) * 30 / 29;
}

static int relation_score(int civ_a, int civ_b) {
    return diplomacy_relation(civ_a, civ_b).relation_score;
}

static void process_creation_vote(AllianceSaveState *state, int a, int b) {
    int ab, ba, years;
    if (!sovereign_alive(b) || alliance_for_civ(b) >= 0 || !no_hard_pair_block(a, b)) {
        state->create_years[a][b] = state->create_years[b][a] = 0;
        return;
    }
    ab = relation_score(a, b); ba = relation_score(b, a);
    if (ab < 95 || ba < 95) {
        state->create_years[a][b] = state->create_years[b][a] = 0;
        return;
    }
    years = ++state->create_years[a][b];
    state->create_years[b][a] = years;
    if (years >= 80 && ((years - 80) % 5) == 0 &&
        vote_roll(create_chance(ab)) && vote_roll(create_chance(ba))) {
        alliance_debug_create_pair(a, b, 0);
    }
}

static int can_join_alliance(int candidate, int alliance_id) {
    int i, count = alliance_member_count(alliance_id);
    AllianceSaveState *state = alliance_internal_state();
    if (!sovereign_alive(candidate) || alliance_for_civ(candidate) >= 0 || count < 2) return 0;
    if (state->voluntary_cooldown[alliance_id][candidate] > 0 ||
        state->kicked_cooldown[alliance_id][candidate] > 0) return 0;
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        if (!sovereign_alive(member) || !no_hard_pair_block(candidate, member)) return 0;
        if (relation_score(candidate, member) < 60) return 0;
    }
    return 1;
}

static void process_join_vote(AllianceSaveState *state, int candidate, int id) {
    int years, i, all_yes = 1, count = alliance_member_count(id);
    if (!can_join_alliance(candidate, id)) {
        state->join_years[candidate][id] = 0;
        return;
    }
    years = ++state->join_years[candidate][id];
    if (years < 20 || ((years - 20) % 5) != 0) return;
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(id, i);
        if (!vote_roll(join_chance(relation_score(member, candidate)))) {
            all_yes = 0;
            break;
        }
    }
    if (all_yes) alliance_debug_add_member(id, candidate, 0);
}

static int kick_eligible(int alliance_id, int target, int *score_sum_out) {
    int i, count = alliance_member_count(alliance_id), sum = 0, any_low = 0;
    if (count < 2 || !alliance_is_formal_member(alliance_id, target)) return 0;
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        int score;
        if (member == target) continue;
        score = relation_score(member, target);
        sum += score;
        if (score < 60) any_low = 1;
    }
    if (score_sum_out) *score_sum_out = sum;
    return any_low && sum < (count - 1) * 60;
}

static void process_kick_vote(AllianceSaveState *state, int id, int target, int count) {
    int years, i, yes = 0, voters = 0;
    if (!kick_eligible(id, target, 0)) {
        state->kick_years[id][target] = 0;
        return;
    }
    years = ++state->kick_years[id][target];
    if (years < 20 || ((years - 20) % 5) != 0) return;
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(id, i);
        if (member == target) continue;
        voters++;
        if (vote_roll(kick_yes_chance(relation_score(member, target)))) yes++;
    }
    if (voters > 0 && yes * 2 > voters) alliance_debug_kick_member(id, target, 100);
}

static void record_alliance_year_step_ms(DWORD start) {
    alliance_year_last_ms = (int)(GetTickCount() - start);
    if (alliance_year_last_ms > alliance_year_peak_ms) alliance_year_peak_ms = alliance_year_last_ms;
    profiler_record_spike_phase(PROFILER_SPIKE_ANNUAL_ALLIANCE,
                                "Alliance Year step", alliance_year_last_ms);
}

void alliance_year_work_begin(AllianceYearWork *work) {
    if (!work) return;
    work->phase = ALLIANCE_YEAR_DECAY_VOLUNTARY;
    work->row = 0; work->col = 0; work->civ_a = 0; work->civ_b = 1;
    work->candidate = 0; work->alliance_id = 0; work->index = 0; work->count = -1;
}

static int step_decay_cell(int matrix[ALLIANCE_MAX][MAX_CIVS], AllianceYearWork *work) {
    if (work->row >= ALLIANCE_MAX) return 0;
    if (matrix[work->row][work->col] > 0) matrix[work->row][work->col]--;
    work->col++;
    if (work->col >= MAX_CIVS) { work->col = 0; work->row++; }
    return 1;
}

static int step_creation(AllianceSaveState *state, AllianceYearWork *work) {
    int limit = min(civ_count, MAX_CIVS);
    while (work->civ_a < limit) {
        int b;
        if (!sovereign_alive(work->civ_a) || alliance_for_civ(work->civ_a) >= 0) {
            work->civ_a++; work->civ_b = work->civ_a + 1; continue;
        }
        if (work->civ_b >= limit) { work->civ_a++; work->civ_b = work->civ_a + 1; continue; }
        b = work->civ_b++;
        process_creation_vote(state, work->civ_a, b);
        return 1;
    }
    return 0;
}

static int step_join(AllianceSaveState *state, AllianceYearWork *work) {
    int limit = min(civ_count, MAX_CIVS);
    while (work->candidate < limit) {
        int id;
        if (!sovereign_alive(work->candidate) || alliance_for_civ(work->candidate) >= 0) {
            work->candidate++; work->alliance_id = 0; continue;
        }
        if (work->alliance_id >= state->next_id || work->alliance_id >= ALLIANCE_MAX) {
            work->candidate++; work->alliance_id = 0; continue;
        }
        id = work->alliance_id++;
        process_join_vote(state, work->candidate, id);
        return 1;
    }
    return 0;
}

static int step_kick(AllianceSaveState *state, AllianceYearWork *work) {
    while (work->alliance_id < state->next_id && work->alliance_id < ALLIANCE_MAX) {
        int target;
        if (work->count < 0) work->count = alliance_member_count(work->alliance_id);
        if (work->index >= work->count) {
            work->alliance_id++; work->index = 0; work->count = -1; continue;
        }
        target = alliance_formal_member_at(work->alliance_id, work->index++);
        process_kick_vote(state, work->alliance_id, target, work->count);
        return 1;
    }
    return 0;
}

static void alliance_year_next_phase(AllianceYearWork *work, int phase) {
    work->phase = phase;
    work->row = 0; work->col = 0; work->civ_a = 0; work->civ_b = 1;
    work->candidate = 0; work->alliance_id = 0; work->index = 0; work->count = -1;
}

int alliance_update_year_step(AllianceYearWork *work, int work_budget) {
    AllianceSaveState *state = alliance_internal_state();
    DWORD start = GetTickCount();
    int remaining = work_budget < 1 ? 1 : work_budget;
    if (!state || !work) return 1;
    while (remaining > 0 && work->phase != ALLIANCE_YEAR_DONE) {
        switch (work->phase) {
            case ALLIANCE_YEAR_DECAY_VOLUNTARY:
                if (step_decay_cell(state->voluntary_cooldown, work)) remaining--;
                else alliance_year_next_phase(work, ALLIANCE_YEAR_DECAY_KICKED);
                break;
            case ALLIANCE_YEAR_DECAY_KICKED:
                if (step_decay_cell(state->kicked_cooldown, work)) remaining--;
                else alliance_year_next_phase(work, ALLIANCE_YEAR_PRE_SANITIZE);
                break;
            case ALLIANCE_YEAR_PRE_SANITIZE:
                alliance_sanitize_loaded();
                alliance_year_next_phase(work, ALLIANCE_YEAR_CREATION);
                break;
            case ALLIANCE_YEAR_CREATION:
                if (step_creation(state, work)) remaining--;
                else alliance_year_next_phase(work, ALLIANCE_YEAR_JOIN);
                break;
            case ALLIANCE_YEAR_JOIN:
                if (step_join(state, work)) remaining--;
                else alliance_year_next_phase(work, ALLIANCE_YEAR_KICK);
                break;
            case ALLIANCE_YEAR_KICK:
                if (step_kick(state, work)) remaining--;
                else alliance_year_next_phase(work, ALLIANCE_YEAR_POST_SANITIZE);
                break;
            case ALLIANCE_YEAR_POST_SANITIZE:
                alliance_sanitize_loaded();
                alliance_year_next_phase(work, ALLIANCE_YEAR_DONE);
                break;
        }
    }
    record_alliance_year_step_ms(start);
    return work->phase == ALLIANCE_YEAR_DONE;
}

void alliance_update_year(void) {
    AllianceYearWork work;
    alliance_year_work_begin(&work);
    while (!alliance_update_year_step(&work, 128)) {}
}

int alliance_year_last_step_ms(void) { return alliance_year_last_ms; }
int alliance_year_peak_step_ms(void) { return alliance_year_peak_ms; }
