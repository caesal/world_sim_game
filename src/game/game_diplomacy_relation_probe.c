#include "game/game_diplomacy_relation_probe.h"

#include "core/game_state.h"
#include "sim/alliance.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_policy.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/diplomacy_stability.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/war_desire.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ui/ui_types.h"

#include <stdlib.h>
#include <string.h>

static void reset_probe_world_tiles(void) {
    int x, y;
    pending_map_size = MAP_SIZE_SMALL;
    set_active_map_size(pending_map_size);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            memset(&world[y][x], 0, sizeof(world[y][x]));
            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].region_id = -1;
        }
    }
    world[2][2].geography = GEO_PLAIN;
    world[2][2].climate = CLIMATE_CONTINENTAL;
    world[2][2].owner = 0;
    world[2][2].province_id = 0;
    world[2][3].geography = GEO_PLAIN;
    world[2][3].climate = CLIMATE_CONTINENTAL;
    world[2][3].owner = 1;
    world[2][3].province_id = 1;
}

static void init_relation_probe_civ(int id, const char *name, int heritage, int military) {
    civilization_reset_slot_state(id);
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].heritage = heritage;
    civs[id].aggression = 2;
    civs[id].governance = 7;
    civs[id].cohesion = 7;
    civs[id].production = 6;
    civs[id].military = military;
    civs[id].logistics = 6;
    civs[id].commerce = 7;
    civs[id].capital_city = id;
}

static void reset_relation_probe_fixture(void) {
    int i;
    diplomacy_reset();
    war_reset();
    diplomacy_relation_score_reset();
    simulation_reset_state();
    memset(cities, 0, sizeof(cities));
    civ_count = 3;
    city_count = 3;
    region_count = 0;
    world_generated = 1;
    reset_probe_world_tiles();
    for (i = 0; i < civ_count; i++) {
        init_relation_probe_civ(i, i == 0 ? "Relation Probe A" :
                                   i == 1 ? "Relation Probe B" : "Relation Probe C",
                                CIV_HERITAGE_WESTERN, i == 2 ? 12 : 5);
        cities[i].alive = 1;
        cities[i].owner = i;
        cities[i].x = 2 + i;
        cities[i].y = 2;
        cities[i].capital = 1;
        population_init_city(i, 4000);
    }
    population_sync_all();
}

static DiplomacyRelation relation_fixture(int state, int score, int trade, int tension) {
    DiplomacyRelation r;
    memset(&r, 0, sizeof(r));
    r.state = (DiplomacyStatus)state;
    r.relation_score = score;
    r.trade_fit = trade;
    r.border_tension = tension;
    r.resource_conflict = 5;
    r.contact_kind = DIP_CONTACT_LAND_BORDER;
    r.years_known = 31;
    r.overlord = -1;
    r.vassal = -1;
    r.last_war_winner = -1;
    r.last_war_loser = -1;
    r.last_war_result = DIP_LAST_WAR_NONE;
    return r;
}

static int has_factor(DiplomacyRelationBreakdown b, int factor_id) {
    int i;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        if (b.factor_ids[i] == factor_id) return 1;
    }
    return 0;
}

static int factor_delta(DiplomacyRelationBreakdown b, int factor_id) {
    int i;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        if (b.factor_ids[i] == factor_id) return b.factor_delta_x100[i];
    }
    return 0;
}

static int factor_sum(DiplomacyRelationBreakdown b) {
    int i, sum = 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) sum += b.factor_delta_x100[i];
    return sum;
}

static int count_peace_relations_from_zero(void) {
    int i, count = 0;
    for (i = 1; i < civ_count; i++) {
        if (diplomacy_relation(0, i).state == DIPLOMACY_PEACE) count++;
    }
    return count;
}

static int case_directional_scores(FILE *summary) {
    DiplomacyRelation ab;
    DiplomacyRelation ba;
    reset_relation_probe_fixture();
    ab = relation_fixture(DIPLOMACY_PEACE, 74, 80, 10);
    ba = relation_fixture(DIPLOMACY_PEACE, 10, 10, 80);
    ba.resource_conflict = 80;
    ba.years_known = 1;
    diplomacy_relation_score_begin_year();
    ab.relation_score = diplomacy_relation_score_apply_year(0, 1, ab);
    ba.relation_score = diplomacy_relation_score_apply_year(1, 0, ba);
    diplomacy_relation_score_end_year();
    fprintf(summary,
            "case=directional_relation_scores our=%d their=%d our_delta_x100=%d their_delta_x100=%d\n",
            ab.relation_score, ba.relation_score,
            diplomacy_relation_breakdown(0, 1).yearly_delta_x100,
            diplomacy_relation_breakdown(1, 0).yearly_delta_x100);
    return ab.relation_score > ba.relation_score &&
           diplomacy_relation_breakdown(0, 1).yearly_delta_x100 > 0 &&
           diplomacy_relation_breakdown(1, 0).yearly_delta_x100 < 0;
}

static int case_relation_factors(FILE *summary) {
    DiplomacyRelation relation;
    DiplomacyRelationBreakdown b;
    reset_relation_probe_fixture();
    relation = relation_fixture(DIPLOMACY_PEACE, 20, 82, 12);
    relation.years_known = 200;
    diplomacy_relation_score_begin_year();
    relation.relation_score = diplomacy_relation_score_apply_year(0, 1, relation);
    diplomacy_relation_score_end_year();
    b = diplomacy_relation_breakdown(0, 1);
    fprintf(summary,
            "case=relation_factors score=%d delta_x100=%d has_trade=%d has_long_peace=%d pairs=%d update_ms=%d\n",
            relation.relation_score, b.yearly_delta_x100, has_factor(b, DIP_REL_FACTOR_TRADE),
            has_factor(b, DIP_REL_FACTOR_LONG_PEACE),
            diplomacy_relation_score_last_pairs(), diplomacy_relation_score_last_update_ms());
    return b.yearly_delta_x100 >= 400 && has_factor(b, DIP_REL_FACTOR_TRADE) &&
           has_factor(b, DIP_REL_FACTOR_LONG_PEACE);
}

static int case_factor_tuning(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, 0, 0, 0);
    DiplomacyRelationBreakdown b;
    int ok = 1, contact, alliance, heritage_bonus, heritage_annual;
    reset_relation_probe_fixture();
    r.years_known = 1; r.resource_conflict = 0;
    diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r);
    b = diplomacy_relation_breakdown(0, 1);
    contact = factor_delta(b, DIP_REL_FACTOR_CONTACT);
    r.state = DIPLOMACY_ALLIANCE;
    diplomacy_relation_score_apply_year(0, 1, r);
    b = diplomacy_relation_breakdown(0, 1);
    alliance = factor_delta(b, DIP_REL_FACTOR_ALLIANCE);
    ok &= contact == 50 && alliance == 100 && !has_factor(b, DIP_REL_FACTOR_CONTACT);
    diplomacy_relation_score_end_year();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    heritage_bonus = diplomacy_relation(0, 1).relation_score;
    r = diplomacy_relation(0, 1);
    diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r);
    heritage_annual = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_HERITAGE);
    diplomacy_relation_score_end_year();
    fprintf(summary, "case=factor_tuning contact=%d alliance=%d heritage_bonus=%d heritage_annual=%d\n",
            contact, alliance, heritage_bonus, heritage_annual);
    return ok && heritage_bonus == 15 && heritage_annual == 10;
}

static int case_full_ledger_preserves_small_factors(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, 0, 85, 90);
    DiplomacyRelationBreakdown b;
    int contact, heritage, sum;
    reset_relation_probe_fixture();
    r.years_known = 200;
    r.border_tension = 90;
    r.resource_conflict = 90;
    r.last_war_result = DIP_LAST_WAR_MILITARY;
    r.easing_years = 12;
    civs[0].military = 5;
    civs[1].military = 80;
    civs[2].military = 240;
    diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r);
    b = diplomacy_relation_breakdown(0, 1);
    diplomacy_relation_score_end_year();
    contact = factor_delta(b, DIP_REL_FACTOR_CONTACT);
    heritage = factor_delta(b, DIP_REL_FACTOR_HERITAGE);
    sum = factor_sum(b);
    fprintf(summary,
            "case=full_ledger_preserves_small_factors contact=%d heritage=%d sum=%d yearly=%d other_x100=%d slots=%d\n",
            contact, heritage, sum, b.yearly_delta_x100, b.yearly_delta_x100 - sum,
            DIP_REL_FACTOR_SLOTS);
    return contact == 50 && heritage == 10 && sum == b.yearly_delta_x100 &&
           has_factor(b, DIP_REL_FACTOR_TRADE) && has_factor(b, DIP_REL_FACTOR_LONG_PEACE) &&
           has_factor(b, DIP_REL_FACTOR_SHARED_THREAT) && has_factor(b, DIP_REL_FACTOR_POWER) &&
           has_factor(b, DIP_REL_FACTOR_RESOURCE) && has_factor(b, DIP_REL_FACTOR_BORDER) &&
           has_factor(b, DIP_REL_FACTOR_WAR_MEMORY);
}

static int case_long_peace_thresholds(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, 0, 0, 0);
    int d74, d75, d200;
    reset_relation_probe_fixture();
    r.contact_kind = DIP_CONTACT_NONE; r.resource_conflict = 0;
    diplomacy_relation_score_begin_year();
    r.years_known = 74; diplomacy_relation_score_apply_year(0, 1, r);
    d74 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_LONG_PEACE);
    r.years_known = 75; diplomacy_relation_score_apply_year(0, 1, r);
    d75 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_LONG_PEACE);
    r.years_known = 200; diplomacy_relation_score_apply_year(0, 1, r);
    d200 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_LONG_PEACE);
    diplomacy_relation_score_end_year();
    fprintf(summary, "case=long_peace_thresholds d74=%d d75=%d d200=%d\n", d74, d75, d200);
    return d74 == 0 && d75 == 75 && d200 == 150;
}

static int case_power_contempt_thresholds(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, 0, 0, 0);
    int d150, d250, d400, contempt;
    reset_relation_probe_fixture();
    r.contact_kind = DIP_CONTACT_NONE; r.resource_conflict = 0; r.years_known = 1;
    diplomacy_relation_score_begin_year();
    civs[0].military = 1; civs[1].military = 100; diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r); d150 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_POWER);
    civs[1].military = 140; diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r); d250 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_POWER);
    civs[1].military = 400; diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r); d400 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_POWER);
    civs[0].military = 400; civs[1].military = 1; diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r); contempt = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_CONTEMPT);
    diplomacy_relation_score_end_year();
    fprintf(summary, "case=power_contempt d150=%d d250=%d d400=%d contempt=%d\n",
            d150, d250, d400, contempt);
    return d150 == -150 && d250 == -250 && d400 == -400 && contempt == -200;
}

static int case_shared_threat(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, 0, 0, 0);
    int shared, direct;
    reset_relation_probe_fixture();
    r.contact_kind = DIP_CONTACT_NONE; r.resource_conflict = 0; r.years_known = 1;
    civs[0].military = 1; civs[1].military = 1; civs[2].military = 400;
    diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r);
    shared = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_SHARED_THREAT);
    civs[2].alive = 0; civs[1].military = 400; diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r);
    direct = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_SHARED_THREAT);
    diplomacy_relation_score_end_year();
    fprintf(summary, "case=shared_threat shared=%d direct_pair=%d\n", shared, direct);
    return shared == 100 && direct == 0;
}

static int case_recovery_and_memory(FILE *summary) {
    DiplomacyRelation r = relation_fixture(DIPLOMACY_PEACE, -20, 0, 0);
    int border45, border75, truce, drift_pos, drift_neg, mem_early, mem_late;
    reset_relation_probe_fixture();
    r.contact_kind = DIP_CONTACT_NONE; r.resource_conflict = 0; r.border_tension = 45;
    diplomacy_relation_score_begin_year();
    diplomacy_relation_score_apply_year(0, 1, r); border45 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_BORDER);
    r.border_tension = 75; diplomacy_relation_score_apply_year(0, 1, r);
    border75 = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_BORDER);
    r.state = DIPLOMACY_TRUCE; r.border_tension = 0; r.truce_years_left = 4; r.relation_score = -1;
    diplomacy_relation_score_apply_year(0, 1, r); truce = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_TRUCE_RECOVERY);
    r.state = DIPLOMACY_PEACE; r.truce_years_left = 0; r.last_war_result = DIP_LAST_WAR_NONE; r.relation_score = 12;
    diplomacy_relation_score_apply_year(0, 1, r); drift_pos = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_QUIET_DRIFT);
    r.relation_score = -12; diplomacy_relation_score_apply_year(0, 1, r);
    drift_neg = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_QUIET_DRIFT);
    r.last_war_result = DIP_LAST_WAR_MILITARY; r.easing_years = 10; diplomacy_relation_score_apply_year(0, 1, r);
    mem_early = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_WAR_MEMORY);
    r.easing_years = 75; diplomacy_relation_score_apply_year(0, 1, r);
    mem_late = factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_WAR_MEMORY);
    diplomacy_relation_score_end_year();
    fprintf(summary, "case=recovery_memory border45=%d border75=%d truce=%d drift=%d/%d memory=%d/%d\n",
            border45, border75, truce, drift_pos, drift_neg, mem_early, mem_late);
    return border45 == -75 && border75 == -150 && truce == 25 &&
           drift_pos == -25 && drift_neg == 25 && mem_early == -75 && mem_late == -25;
}

static int case_truce_grouping_contract(FILE *summary) {
    fprintf(summary,
            "case=diplomacy_tab_grouping tabs=Alliance/Peace/Tense/War/Vassal truce_group=Tense vassal_group=Vassal\n");
    return DIPLOMACY_VIEW_ALLIANCE == 0 && DIPLOMACY_VIEW_PEACE == 1 &&
           DIPLOMACY_VIEW_TENSE == 2 && DIPLOMACY_VIEW_WAR == 3 &&
           DIPLOMACY_VIEW_VASSAL == 4;
}

static int case_directional_refresh_preserves(FILE *summary) {
    DiplomacyRelation ab = relation_fixture(DIPLOMACY_PEACE, 82, 30, 8);
    DiplomacyRelation ba = relation_fixture(DIPLOMACY_PEACE, -27, 30, 8);
    DiplomacyRelation out_ab, out_ba;
    reset_relation_probe_fixture();
    diplomacy_restore_relation(0, 1, ab);
    diplomacy_restore_relation(1, 0, ba);
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    out_ab = diplomacy_relation(0, 1);
    out_ba = diplomacy_relation(1, 0);
    fprintf(summary,
            "case=directional_refresh_preserves ab=%d ba=%d contact=%d border=%d\n",
            out_ab.relation_score, out_ba.relation_score, out_ab.contact_kind, out_ab.border_length);
    return out_ab.relation_score == 82 && out_ba.relation_score == -27 &&
           out_ab.contact_kind == DIP_CONTACT_LAND_BORDER && out_ab.border_length > 0;
}

static int case_high_score_alliance(FILE *summary) {
    DiplomacyRelation ab = relation_fixture(DIPLOMACY_PEACE, 95, 20, 8);
    DiplomacyRelation out_ab;
    int not_94, seed, create_seed = -1, alliance_id = -1;
    reset_relation_probe_fixture();
    ab.relation_score = 94;
    diplomacy_restore_relation(0, 1, ab);
    diplomacy_restore_relation(1, 0, ab);
    diplomacy_mark_contacts_dirty();
    alliance_debug_set_create_years(0, 1, 79);
    alliance_update_year();
    not_94 = alliance_for_civ(0) < 0;
    for (seed = 0; seed < 300 && create_seed < 0; seed++) {
        reset_relation_probe_fixture();
        ab.relation_score = 95;
        diplomacy_restore_relation(0, 1, ab);
        diplomacy_restore_relation(1, 0, ab);
        diplomacy_mark_contacts_dirty();
        alliance_debug_set_create_years(0, 1, 79);
        srand((unsigned int)seed);
        alliance_update_year();
        alliance_id = alliance_for_civ(0);
        if (alliance_id >= 0) create_seed = seed;
    }
    out_ab = diplomacy_relation(0, 1);
    fprintf(summary,
            "case=high_score_alliance mutual94_blocked=%d create_seed=%d alliance=%d final_state=%d ab=%d ba=%d contact=%d\n",
            not_94, create_seed, alliance_id, out_ab.state, out_ab.relation_score,
            diplomacy_relation(1, 0).relation_score, out_ab.contact_kind);
    return not_94 && create_seed >= 0 && alliance_id >= 0 && out_ab.state == DIPLOMACY_ALLIANCE;
}

static int case_alliance_grace_and_exit(FILE *summary) {
    DiplomacyStatus state = DIPLOMACY_ALLIANCE;
    int i, protected_ok = 1;
    reset_relation_probe_fixture();
    diplomacy_stability_force_pair(0, 1, DIPLOMACY_ALLIANCE);
    for (i = 0; i < DIPLOMACY_SOFT_GRACE_YEARS; i++) {
        state = diplomacy_stability_step_pair(0, 1, state, DIPLOMACY_PEACE, 0);
        protected_ok &= state == DIPLOMACY_ALLIANCE &&
                        diplomacy_stability_candidate_years(0, 1) == 0;
    }
    for (i = 0; i < DIPLOMACY_SOFT_TRANSITION_YEARS; i++) {
        state = diplomacy_stability_step_pair(0, 1, state, DIPLOMACY_PEACE, 1);
    }
    fprintf(summary,
            "case=alliance_grace_and_exit protected=%d final_state=%d candidate=%d/%d state_years=%d\n",
            protected_ok, state, diplomacy_stability_candidate_state(0, 1),
            diplomacy_stability_candidate_years(0, 1), diplomacy_stability_state_years(0, 1));
    return protected_ok && state == DIPLOMACY_PEACE &&
           diplomacy_stability_candidate_years(0, 1) == 0;
}

static int case_tense_recovery_stability(FILE *summary) {
    DiplomacyRelation blocked = relation_fixture(DIPLOMACY_TENSE, -24, 20, 86);
    DiplomacyRelation high_score = relation_fixture(DIPLOMACY_TENSE, 73, 20, 100);
    DiplomacyStatus state = DIPLOMACY_TENSE;
    int blocked_recovery, high_recovery, i, before_final;
    reset_relation_probe_fixture();
    blocked.resource_conflict = 30;
    high_score.resource_conflict = 90;
    blocked_recovery = diplomacy_policy_tense_recovery_requested(&blocked, -24, -24);
    high_recovery = diplomacy_policy_tense_recovery_requested(&high_score, 73, 73);
    diplomacy_stability_force_pair(0, 1, DIPLOMACY_TENSE);
    for (i = 0; i < DIPLOMACY_SOFT_GRACE_YEARS; i++) {
        state = diplomacy_stability_step_pair(0, 1, state, DIPLOMACY_TENSE, 1);
    }
    for (i = 0; i < DIPLOMACY_SOFT_TRANSITION_YEARS - 1; i++) {
        state = diplomacy_stability_step_pair(0, 1, state, DIPLOMACY_PEACE, 1);
    }
    before_final = state;
    state = diplomacy_stability_step_pair(0, 1, state, DIPLOMACY_PEACE, 1);
    fprintf(summary,
            "case=tense_recovery_stability blocked_recovery=%d high_score_recovery=%d pressure=%d/%d before_final=%d final_state=%d candidate=%d/%d\n",
            blocked_recovery, high_recovery, high_score.border_tension, high_score.resource_conflict,
            before_final, state, diplomacy_stability_candidate_state(0, 1),
            diplomacy_stability_candidate_years(0, 1));
    return !blocked_recovery && high_recovery && before_final == DIPLOMACY_TENSE &&
           state == DIPLOMACY_PEACE;
}

static int case_peace_grouping_stability(FILE *summary) {
    int first, second;
    DiplomacyRelation ab = relation_fixture(DIPLOMACY_PEACE, 10, 20, 8);
    DiplomacyRelation ba = relation_fixture(DIPLOMACY_PEACE, -15, 20, 8);
    reset_relation_probe_fixture();
    diplomacy_restore_relation(0, 1, ab);
    diplomacy_restore_relation(1, 0, ba);
    first = count_peace_relations_from_zero();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    second = count_peace_relations_from_zero();
    fprintf(summary, "case=peace_grouping_stability first=%d second=%d\n", first, second);
    return first == second && first == 1;
}

static int case_alliance_block_reason(FILE *summary) {
    DiplomacyRelation relation = relation_fixture(DIPLOMACY_TRUCE, 90, 5, 5);
    const char *reason = diplomacy_policy_alliance_block_reason(&relation, relation.contact_kind);
    fprintf(summary, "case=alliance_block_reason reason=%s blocked=%d\n", reason,
            diplomacy_policy_alliance_hard_blocked(&relation, relation.contact_kind));
    return strcmp(reason, "truce") == 0;
}

static int case_four_heritage_affinity(FILE *summary) {
    DiplomacyRelation r;
    int same_ok = 1, diff_ok = 1, annual_ok = 1, war_ok = 1;
    for (int h = 0; h < CIV_HERITAGE_COUNT; h++) {
        reset_relation_probe_fixture();
        civs[0].heritage = h; civs[1].heritage = h;
        diplomacy_mark_contacts_dirty(); diplomacy_update_contacts();
        same_ok &= diplomacy_relation(0, 1).relation_score == 15;
        war_ok &= war_desire_calculate(0, 1, diplomacy_relation(0, 1)).heritage_affinity_penalty == 8;
        r = diplomacy_relation(0, 1);
        diplomacy_relation_score_begin_year();
        diplomacy_relation_score_apply_year(0, 1, r);
        annual_ok &= factor_delta(diplomacy_relation_breakdown(0, 1), DIP_REL_FACTOR_HERITAGE) == 10;
        diplomacy_relation_score_end_year();
        reset_relation_probe_fixture();
        civs[0].heritage = h; civs[1].heritage = (h + 1) % CIV_HERITAGE_COUNT;
        diplomacy_mark_contacts_dirty(); diplomacy_update_contacts();
        diff_ok &= diplomacy_relation(0, 1).relation_score == 0;
        war_ok &= war_desire_calculate(0, 1, diplomacy_relation(0, 1)).heritage_affinity_penalty == 0;
    }
    fprintf(summary, "case=four_heritage_affinity same_bonus=%d annual=%d war=%d different_none=%d count=%d\n",
            same_ok, annual_ok, war_ok, diff_ok, CIV_HERITAGE_COUNT);
    return same_ok && annual_ok && war_ok && diff_ok && CIV_HERITAGE_COUNT == 4;
}

int run_diplomacy_relation_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_directional_scores(summary);
    ok &= case_relation_factors(summary);
    ok &= case_factor_tuning(summary);
    ok &= case_full_ledger_preserves_small_factors(summary);
    ok &= case_long_peace_thresholds(summary);
    ok &= case_power_contempt_thresholds(summary);
    ok &= case_shared_threat(summary);
    ok &= case_recovery_and_memory(summary);
    ok &= case_truce_grouping_contract(summary);
    ok &= case_directional_refresh_preserves(summary);
    ok &= case_high_score_alliance(summary);
    ok &= case_alliance_grace_and_exit(summary);
    ok &= case_tense_recovery_stability(summary);
    ok &= case_peace_grouping_stability(summary);
    ok &= case_alliance_block_reason(summary);
    ok &= case_four_heritage_affinity(summary);
    return ok;
}
