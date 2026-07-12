#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/world_announcement.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int civ_id;
    int order;
    long long army;
    long long population;
    long long provinces;
} UnionProposer;

static int alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive;
}

int alliance_union_required_years_for_type(int alliance_type) {
    return alliance_type == ALLIANCE_TYPE_MILITARY ? ALLIANCE_UNION_MILITARY_YEARS :
           ALLIANCE_UNION_DEFENSIVE_YEARS;
}

static int collect_members(const AllianceRecord *record, int *members, int *latest_join_year) {
    int i, count = 0, latest = record ? record->founded_year : 0;
    if (!record || !record->active) return 0;
    for (i = 0; i < record->member_count && i < MAX_CIVS; i++) {
        int civ_id = record->members[i];
        if (!alive_civ(civ_id)) continue;
        members[count++] = civ_id;
        if (record->joined_year_by_civ[civ_id] > latest) latest = record->joined_year_by_civ[civ_id];
    }
    if (latest_join_year) *latest_join_year = latest;
    return count;
}

static int joined_year_for_member(const AllianceRecord *record, int civ_id) {
    if (!record || civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    return record->joined_year_by_civ[civ_id] >= 0 ? record->joined_year_by_civ[civ_id] :
           record->founded_year;
}

static int member_order_index(const AllianceRecord *record, int civ_id) {
    int i;
    for (i = 0; record && i < record->member_count && i < MAX_CIVS; i++)
        if (record->members[i] == civ_id) return i;
    return MAX_CIVS;
}

static int member_before(const AllianceRecord *record, int a, int b) {
    int ay = joined_year_for_member(record, a), by = joined_year_for_member(record, b);
    int ao = member_order_index(record, a), bo = member_order_index(record, b);
    if (ay != by) return ay < by;
    if (ao != bo) return ao < bo;
    return a < b;
}

static void sort_members_by_join_order(const AllianceRecord *record, int *members, int count) {
    int i;
    for (i = 1; i < count; i++) {
        int value = members[i], j = i - 1;
        while (j >= 0 && member_before(record, value, members[j])) {
            members[j + 1] = members[j];
            j--;
        }
        members[j + 1] = value;
    }
}

static long long owned_province_count(int civ_id) {
    int i;
    long long count = 0;
    for (i = 0; i < region_count; i++)
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    return count;
}

static UnionProposer make_proposer(int civ_id, int order) {
    UnionProposer p;
    p.civ_id = civ_id;
    p.order = order;
    p.army = war_current_soldiers_for_civ(civ_id);
    p.population = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].population : 0;
    p.provinces = owned_province_count(civ_id);
    return p;
}

static long long normalized_score(long long value, long long other, int weight) {
    long long denom = max(value, other);
    return denom > 0 ? value * weight * 1000 / denom : 0;
}

static int proposer_compare(const UnionProposer *a, const UnionProposer *b) {
    int a_wins = 0, b_wins = 0;
    long long as, bs;
    if (a->army > b->army) a_wins++; else if (b->army > a->army) b_wins++;
    if (a->population > b->population) a_wins++; else if (b->population > a->population) b_wins++;
    if (a->provinces > b->provinces) a_wins++; else if (b->provinces > a->provinces) b_wins++;
    if (a_wins >= 2 && b_wins < 2) return 1;
    if (b_wins >= 2 && a_wins < 2) return -1;
    as = normalized_score(a->army, b->army, 40) +
         normalized_score(a->population, b->population, 35) +
         normalized_score(a->provinces, b->provinces, 25);
    bs = normalized_score(b->army, a->army, 40) +
         normalized_score(b->population, a->population, 35) +
         normalized_score(b->provinces, a->provinces, 25);
    if (as != bs) return as > bs ? 1 : -1;
    if (a->order != b->order) return a->order < b->order ? 1 : -1;
    return a->civ_id < b->civ_id ? 1 : (a->civ_id > b->civ_id ? -1 : 0);
}

int alliance_union_proposer_cooldown_remaining(int alliance_id, int proposer_civ) {
    AllianceSaveState *state = alliance_internal_state();
    int i, latest = -100000;
    if (!state || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return 0;
    for (i = 0; i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        AllianceVoteRecord *vote = &state->votes[alliance_id][i];
        if (!vote->active || vote->vote_type != ALLIANCE_VOTE_UNION ||
            vote->target_civ_id != proposer_civ || vote->passed ||
            vote->rejection_reason != ALLIANCE_REJECT_VOTE_FAILED) continue;
        if (vote->vote_year > latest) latest = vote->vote_year;
    }
    return latest < 0 ? 0 : max(0, ALLIANCE_UNION_RETRY_YEARS - (year - latest));
}

static int eligible_proposer(int alliance_id, int civ_id, int order, int eligible_year) {
    int start = eligible_year + order * ALLIANCE_UNION_PROPOSER_DELAY_YEARS;
    return alive_civ(civ_id) && year >= start &&
           alliance_union_proposer_cooldown_remaining(alliance_id, civ_id) <= 0;
}

static int select_proposer(const AllianceRecord *record, int alliance_id, int *members,
                           int count, int eligible_year) {
    UnionProposer best;
    int i, have = 0;
    sort_members_by_join_order(record, members, count);
    memset(&best, 0, sizeof(best));
    for (i = 0; i < count; i++) {
        UnionProposer candidate;
        if (!eligible_proposer(alliance_id, members[i], i, eligible_year)) continue;
        candidate = make_proposer(members[i], i);
        if (!have || proposer_compare(&candidate, &best) > 0) {
            best = candidate;
            have = 1;
        }
    }
    return have ? best.civ_id : -1;
}

int alliance_union_vote_yes_chance_from_ratio_permille(int avg_ratio_permille) {
    int r = clamp(avg_ratio_permille, 0, 1000);
    if (r >= 950) return 15;
    if (r >= 700) return clamp(35 - ((r - 700) * 20 + 125) / 250, 15, 35);
    if (r >= 500) return clamp(50 - ((r - 500) * 15 + 100) / 200, 35, 50);
    if (r > 250) return clamp(70 - ((r - 250) * 20 + 125) / 250, 50, 70);
    return 70;
}

static int ratio_permille(long long voter, long long proposer) {
    if (proposer <= 0) return voter <= 0 ? 0 : 1000;
    return (int)min(1000, max(0, voter * 1000 / proposer));
}

static int voter_stronger_all_three(int voter, int proposer) {
    return war_current_soldiers_for_civ(voter) > war_current_soldiers_for_civ(proposer) &&
           civs[voter].population > civs[proposer].population &&
           owned_province_count(voter) > owned_province_count(proposer);
}

static int union_yes_chance(int voter, int proposer) {
    int avg;
    if (voter == proposer) return 100;
    if (voter_stronger_all_three(voter, proposer)) return 0;
    avg = (ratio_permille(war_current_soldiers_for_civ(voter), war_current_soldiers_for_civ(proposer)) +
           ratio_permille(civs[voter].population, civs[proposer].population) +
           ratio_permille(owned_province_count(voter), owned_province_count(proposer))) / 3;
    return alliance_union_vote_yes_chance_from_ratio_permille(avg);
}

static int vote_roll(int chance_percent) {
    return rnd(100) < clamp(chance_percent, 0, 100);
}

static int member_mask_contains(const unsigned char *mask, int civ_id) {
    return civ_id >= 0 && civ_id < MAX_CIVS && mask[civ_id];
}

static int council_units_ready(AllianceSaveState *state, int alliance_id, const int *members, int count) {
    int i, sum = 0;
    if (!state || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return 0;
    for (i = 0; i < count; i++) {
        int member = members[i];
        if (member >= 0 && member < MAX_CIVS)
            sum += state->council_vote_units[alliance_id][member];
    }
    return sum == ALLIANCE_COUNCIL_TOTAL_UNITS;
}

static void transfer_owned_world(int proposer, const unsigned char *absorbed_mask) {
    int x, y, i;
    for (i = 0; i < region_count; i++)
        if (member_mask_contains(absorbed_mask, natural_regions[i].owner_civ))
            natural_regions[i].owner_civ = proposer;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            if (member_mask_contains(absorbed_mask, world[y][x].owner)) world[y][x].owner = proposer;
}

static void transfer_cities_to_proposer(int proposer, const int *members, int count) {
    int i, m;
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive) continue;
        for (m = 0; m < count; m++) {
            if (members[m] == proposer || cities[i].owner != members[m]) continue;
            cities[i].owner = proposer;
            cities[i].capital = 0;
            break;
        }
    }
    if (civs[proposer].capital_city < 0) {
        for (i = 0; i < city_count; i++) {
            if (cities[i].alive && cities[i].owner == proposer) {
                civs[proposer].capital_city = i;
                cities[i].capital = 1;
                break;
            }
        }
    }
}

static void retire_absorbed_members(int proposer, const int *members, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int civ_id = members[i];
        if (civ_id == proposer) continue;
        civs[proposer].treasury += max(0, civs[civ_id].treasury);
        civs[proposer].treasury_cap += max(0, civs[civ_id].treasury_cap);
        civs[proposer].treasury_pending_surplus += max(0, civs[civ_id].treasury_pending_surplus);
        war_end_direct_for_civ(civ_id);
        diplomacy_clear_civ(civ_id);
        civs[civ_id].alive = 0;
        civs[civ_id].population = 0;
        civs[civ_id].territory = 0;
        civs[civ_id].capital_city = -1;
        civs[civ_id].treasury = 0;
        civs[civ_id].treasury_cap = 0;
        civs[civ_id].treasury_pending_surplus = 0;
    }
    civs[proposer].treasury_cap = max(civs[proposer].treasury_cap, civs[proposer].treasury);
}

static void finish_world_refresh(int proposer, const int *members, int count) {
    int i;
    for (i = 0; i < count; i++) world_mark_province_partition_dirty(members[i]);
    world_mark_province_partition_dirty(proposer);
    regions_claim_cache_reset();
    world_recalculate_territory();
    population_sync_all();
    ports_refresh_city_regions();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    dirty_mark_territory();
    dirty_mark_city();
    dirty_mark_population();
    dirty_mark_civ();
    dirty_mark_diplomacy();
    dirty_mark_alliance();
    world_visual_revision++;
}

static void deactivate_alliance(int alliance_id, const int *members, int count) {
    AllianceSaveState *state = alliance_internal_state();
    int i;
    for (i = 0; i < count; i++)
        if (members[i] >= 0 && members[i] < MAX_CIVS) state->civ_alliance[members[i]] = -1;
    state->records[alliance_id].active = 0;
    state->records[alliance_id].member_count = 0;
    alliance_power_cache_reset();
}

static void absorb_members(int alliance_id, int proposer, const int *members, int count) {
    unsigned char absorbed_mask[MAX_CIVS] = {0};
    int i;
    for (i = 0; i < count; i++) {
        if (members[i] == proposer) continue;
        absorbed_mask[members[i]] = 1;
    }
    world_announcement_emit_union(alliance_id, proposer, members, count);
    transfer_owned_world(proposer, absorbed_mask);
    transfer_cities_to_proposer(proposer, members, count);
    retire_absorbed_members(proposer, members, count);
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_UNION_ABSORBED, proposer, -1,
                            ALLIANCE_VOTE_UNION, ALLIANCE_REJECT_NONE);
    deactivate_alliance(alliance_id, members, count);
    finish_world_refresh(proposer, members, count);
}

static void resolve_union_vote(int alliance_id, int proposer, const int *members, int count) {
    AllianceSaveState *state = alliance_internal_state();
    signed char votes[MAX_CIVS];
    int council_units[MAX_CIVS];
    int i, yes = 0, no = 0, passed;
    for (i = 0; i < MAX_CIVS; i++) {
        votes[i] = ALLIANCE_MEMBER_VOTE_NA;
        council_units[i] = 0;
    }
    alliance_record_candidate(alliance_id, proposer, ALLIANCE_CANDIDATE_UNION,
                              ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE, year, 100,
                              ALLIANCE_CANDIDATE_ACTIVE, ALLIANCE_REJECT_NONE);
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_UNION_VOTE_INITIATED, proposer,
                            -1, ALLIANCE_VOTE_UNION, ALLIANCE_REJECT_NONE);
    for (i = 0; i < count; i++) {
        int member = members[i];
        int units = state->council_vote_units[alliance_id][member];
        if (units <= 0) continue;
        council_units[member] = units;
        if (member == proposer || vote_roll(union_yes_chance(member, proposer))) {
            votes[member] = ALLIANCE_MEMBER_VOTE_YES;
            yes += units;
        } else {
            votes[member] = ALLIANCE_MEMBER_VOTE_NO;
            no += units;
        }
    }
    passed = yes * 4 > ALLIANCE_COUNCIL_TOTAL_UNITS * 3;
    alliance_record_weighted_vote(alliance_id, ALLIANCE_VOTE_UNION, proposer, -1,
                                  votes, council_units, yes, no, passed,
                                  passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_candidate(alliance_id, proposer, ALLIANCE_CANDIDATE_UNION,
                              ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE, year, 100,
                              passed ? ALLIANCE_CANDIDATE_PASSED : ALLIANCE_CANDIDATE_ACTIVE,
                              passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_history(alliance_id, passed ? ALLIANCE_HISTORY_UNION_VOTE_PASSED :
                            ALLIANCE_HISTORY_UNION_VOTE_FAILED, proposer, -1,
                            ALLIANCE_VOTE_UNION,
                            passed ? ALLIANCE_REJECT_NONE : ALLIANCE_REJECT_VOTE_FAILED);
    if (passed) absorb_members(alliance_id, proposer, members, count);
}

int alliance_union_try(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceRecord *record;
    int members[MAX_CIVS], count, latest_join, eligible_year, proposer;
    if (!state || alliance_id < 0 || alliance_id >= state->next_id || alliance_id >= ALLIANCE_MAX) return 0;
    record = &state->records[alliance_id];
    if (!record->active) return 0;
    count = collect_members(record, members, &latest_join);
    if (count < 2) return 0;
    eligible_year = latest_join + alliance_union_required_years_for_type(state->alliance_type[alliance_id]);
    if (year < eligible_year) return 0;
    if (!council_units_ready(state, alliance_id, members, count))
        alliance_council_recalculate(alliance_id, 0);
    proposer = select_proposer(record, alliance_id, members, count, eligible_year);
    if (proposer < 0) return 0;
    resolve_union_vote(alliance_id, proposer, members, count);
    return 1;
}

int alliance_union_update_year_step(AllianceYearWork *work) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || !work) return 0;
    while (work->alliance_id < state->next_id && work->alliance_id < ALLIANCE_MAX) {
        int id = work->alliance_id++;
        alliance_union_try(id);
        return 1;
    }
    return 0;
}
