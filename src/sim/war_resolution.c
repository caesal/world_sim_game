#include "sim/war_resolution.h"

#include "core/dirty_flags.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/regions.h"
#include "sim/regions_settlement.h"
#include "sim/sea_lanes.h"
#include "sim/simulation.h"
#include "sim/vassal.h"

#include <stdlib.h>

static int is_valid_civ_id(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int resource_deficit_value(int value, int target) {
    return clamp(target - value, 0, target);
}

static int region_borders_owner(int region_id, int owner) {
    return regions_region_has_owner_neighbor(region_id, owner);
}

static int region_value_for_winner(int region_id, int winner) {
    const NaturalRegion *region = regions_get(region_id);
    CountrySummary country = summarize_country(winner);
    TerrainStats stats;
    int score;

    if (!region || !region->alive) return -1000000;
    stats = region->average_stats;
    score = region->tile_count / 2 + region->development_score / 2 + region->habitability * 4;
    score += stats.food * resource_deficit_value(country.food, 5) * 2;
    score += stats.water * resource_deficit_value(country.water, 5) * 3;
    score += stats.minerals * resource_deficit_value(country.minerals, 5) * 2;
    score += stats.wood * resource_deficit_value(country.wood, 5) * 2;
    score += stats.stone * resource_deficit_value(country.stone, 5) * 2;
    score += stats.money * 3 + region->resource_diversity * 4;
    if (region->has_port_site) score += 18;
    return score + rnd(16);
}

static int any_city_owned_by(int owner) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == owner) return 1;
    }
    return 0;
}

static int capital_candidate_score(int city_id, int owner, int enemy) {
    RegionSummary province = summarize_city_region(city_id);
    int region_id = regions_region_for_city(city_id);
    int score;

    if (!cities[city_id].alive || cities[city_id].owner != owner) return -1000000;
    score = cities[city_id].population / 2 + province.food * 4 + province.water * 4 +
            province.money * 3 + province.habitability * 4 + province.tiles;
    if (cities[city_id].port) score += 20;
    if (region_borders_owner(region_id, enemy)) score -= 60;
    return score;
}

static void choose_new_capital(int loser, int winner) {
    int i;
    int best = -1;
    int best_score = -1000000;
    int city_visual_changed = 0;

    for (i = 0; i < city_count; i++) {
        int score;
        if (cities[i].alive && cities[i].owner == loser) {
            if (cities[i].capital) city_visual_changed = 1;
            cities[i].capital = 0;
        }
        score = capital_candidate_score(i, loser, winner);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0) {
        if (!cities[best].capital) city_visual_changed = 1;
        cities[best].capital = 1;
        civs[loser].capital_city = best;
    } else {
        civs[loser].capital_city = -1;
        civs[loser].alive = 0;
    }
    if (city_visual_changed) dirty_mark_city();
}

static void handle_capital_loss(int loser, int winner) {
    disorder_add_war_pressure(loser, 18);
    civs[loser].cohesion = clamp(civs[loser].cohesion - 1, 0, 10);
    if (!any_city_owned_by(loser)) {
        civs[loser].alive = 0;
        civs[loser].capital_city = -1;
        return;
    }
    choose_new_capital(loser, winner);
}

static int owner_in_loser_side(int loser, int owner) {
    return owner == loser || vassal_is_direct(loser, owner);
}

static int side_owned_province_count(int loser) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && owner_in_loser_side(loser, natural_regions[i].owner_civ)) count++;
    }
    return count;
}

static int owner_can_cede_region(int loser, int owner) {
    if (vassal_is_direct(loser, owner) && war_owned_province_count(owner) <= 1) return 0;
    return 1;
}

static int region_point_x(const NaturalRegion *region) {
    return region->capital_x >= 0 ? region->capital_x : region->center_x;
}

static int region_point_y(const NaturalRegion *region) {
    return region->capital_y >= 0 ? region->capital_y : region->center_y;
}

static int winner_capital_distance(int region_id, int winner) {
    const NaturalRegion *region = regions_get(region_id);
    int capital = civs[winner].capital_city;
    if (!region) return 1000000;
    if (capital < 0 || capital >= city_count || !cities[capital].alive) return 1000000;
    return abs(region_point_x(region) - cities[capital].x) +
           abs(region_point_y(region) - cities[capital].y);
}

static int region_port_city(int region_id, int owner) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != owner || !cities[i].port) continue;
        if (regions_region_for_city(i) == region_id) return i;
    }
    return -1;
}

static int region_preferred_transfer_city(int region_id, int owner) {
    const NaturalRegion *region = regions_get(region_id);
    int capital = owner >= 0 && owner < civ_count ? civs[owner].capital_city : -1;
    int i;

    if (regions_city_is_local_to_region(capital, region_id)) return capital;
    if (region && regions_city_is_local_to_region(region->city_id, region_id)) return region->city_id;
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == owner && regions_region_for_city(i) == region_id) return i;
    }
    return -1;
}

static int region_port_distance_to_winner(int region_id, int winner) {
    const NaturalRegion *region = regions_get(region_id);
    int port_city;
    int best = 1000000;
    int px;
    int py;
    int i;

    if (!region) return best;
    port_city = region_port_city(region_id, region->owner_civ);
    if (port_city < 0) return best;
    px = cities[port_city].port_x >= 0 ? cities[port_city].port_x : cities[port_city].x;
    py = cities[port_city].port_y >= 0 ? cities[port_city].port_y : cities[port_city].y;
    for (i = 0; i < city_count; i++) {
        int wx, wy, distance;
        if (!cities[i].alive || cities[i].owner != winner || !cities[i].port) continue;
        if (!sea_lanes_network_connected(i, port_city)) continue;
        wx = cities[i].port_x >= 0 ? cities[i].port_x : cities[i].x;
        wy = cities[i].port_y >= 0 ? cities[i].port_y : cities[i].y;
        distance = abs(px - wx) + abs(py - wy);
        if (distance < best) best = distance;
    }
    return best;
}

static int pick_side_land_region(int loser, int winner) {
    int i;
    int best = -1;
    int best_distance = 1000000;
    int best_value = -1000000;

    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = &natural_regions[i];
        int owner;
        int distance;
        int value;
        if (!region->alive) continue;
        owner = region->owner_civ;
        if (!owner_in_loser_side(loser, owner) || !owner_can_cede_region(loser, owner)) continue;
        if (!region_borders_owner(i, winner)) continue;
        distance = winner_capital_distance(i, winner);
        value = region_value_for_winner(i, winner);
        if (distance < best_distance || (distance == best_distance && value > best_value)) {
            best_distance = distance;
            best_value = value;
            best = i;
        }
    }
    return best;
}

static int pick_side_port_region(int loser, int winner) {
    int i;
    int best = -1;
    int best_distance = 1000000;
    int best_value = -1000000;
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = &natural_regions[i];
        int owner;
        int distance;
        int value;
        if (!region->alive) continue;
        owner = region->owner_civ;
        if (!owner_in_loser_side(loser, owner) || !owner_can_cede_region(loser, owner)) continue;
        distance = region_port_distance_to_winner(i, winner);
        if (distance >= 1000000) continue;
        value = region_value_for_winner(i, winner);
        if (distance < best_distance || (distance == best_distance && value > best_value)) {
            best_distance = distance;
            best_value = value;
            best = i;
        }
    }
    return best;
}

static int pick_side_cession_region(int loser, int winner) {
    int region_id = pick_side_land_region(loser, winner);
    return region_id >= 0 ? region_id : pick_side_port_region(loser, winner);
}

static int transfer_side_border_regions(int loser, int winner, int count) {
    int transferred = 0;

    while (transferred < count) {
        int region_id = pick_side_cession_region(loser, winner);
        const NaturalRegion *region = regions_get(region_id);
        int region_owner;
        int capital_city;
        int capital_lost;
        int preferred_city;
        if (!region) break;
        region_owner = region->owner_civ;
        if (!owner_in_loser_side(loser, region_owner)) break;
        capital_city = civs[region_owner].capital_city;
        capital_lost = regions_region_for_city(capital_city) == region_id;
        preferred_city = region_preferred_transfer_city(region_id, region_owner);
        if (!regions_claim_for_civ(region_id, winner, preferred_city, 0)) break;
        if (capital_lost) {
            civs[region_owner].capital_city = -1;
            if (capital_city >= 0 && capital_city < city_count && cities[capital_city].capital) {
                cities[capital_city].capital = 0;
                dirty_mark_city();
            }
        }
        disorder_add_war_pressure(region_owner, 12);
        transferred++;
        if (capital_lost) handle_capital_loss(region_owner, winner);
    }
    return transferred;
}

static int cession_count_from_loss(int loser, int winner, int casualties, int initial_soldiers) {
    int side_count = side_owned_province_count(loser);
    int war_loss_cap;
    int winner_capacity_cap;
    int loss_based;
    if (side_count <= 0) return 0;
    war_loss_cap = max(1, side_count / 5);
    winner_capacity_cap = max(1, war_owned_province_count(winner) / 5);
    if (initial_soldiers <= 0) {
        loss_based = 1;
    } else {
        long long numerator = (long long)war_loss_cap * max(0, casualties) * 3;
        long long denominator = (long long)initial_soldiers * 2;
        if (denominator < 1) denominator = 1;
        loss_based = (int)((numerator + denominator - 1) / denominator);
        loss_based = clamp(loss_based, 1, war_loss_cap);
    }
    return min(loss_based, min(war_loss_cap, winner_capacity_cap));
}

static int apply_indemnity_offset(int loser, int winner, int planned_count, int *out_offsets, int *out_spent) {
    int max_offsets;
    int cost_per;
    int offsets;
    int spent;
    if (out_offsets) *out_offsets = 0;
    if (out_spent) *out_spent = 0;
    if (planned_count <= 1 || !is_valid_civ_id(loser) || !is_valid_civ_id(winner)) return planned_count;
    max_offsets = planned_count / 2;
    cost_per = economy_indemnity_cost_per_province(loser, winner);
    if (cost_per <= 0) return planned_count;
    offsets = min(max_offsets, max(0, civs[loser].treasury) / cost_per);
    if (offsets <= 0) return planned_count;
    spent = economy_spend_treasury(loser, offsets * cost_per);
    offsets = spent / cost_per;
    if (offsets <= 0) return planned_count;
    if (out_offsets) *out_offsets = offsets;
    if (out_spent) *out_spent = spent;
    return planned_count - offsets;
}

int war_owned_province_count(int civ_id) {
    return regions_owned_count_for_civ(civ_id);
}

void war_apply_outcome_with_result(int attacker, int defender, WarOutcome outcome, int margin,
                                   int loser_casualties, int loser_initial_soldiers, int last_war_result) {
    int winner = -1;
    int loser = -1;

    if (outcome == WAR_OUTCOME_ATTACKER_WIN) {
        winner = attacker;
        loser = defender;
    } else if (outcome == WAR_OUTCOME_DEFENDER_WIN) {
        winner = defender;
        loser = attacker;
    }
    if (winner >= 0 && loser >= 0 && is_valid_civ_id(winner) && is_valid_civ_id(loser)) {
        int cession_count = cession_count_from_loss(loser, winner, loser_casualties, loser_initial_soldiers);
        int transferred;
        int indemnity_offsets = 0;
        int indemnity_spent = 0;
        diplomacy_record_war_result_kind(winner, loser, (DiplomacyLastWarResult)last_war_result);
        cession_count = apply_indemnity_offset(loser, winner, cession_count,
                                               &indemnity_offsets, &indemnity_spent);
        transferred = transfer_side_border_regions(loser, winner, cession_count);
        if (indemnity_offsets > 0) {
            event_log_push_structured(EVENT_TYPE_TREASURY_INDEMNITY, EVENT_SEVERITY_WARNING,
                                      loser, winner, indemnity_spent, -1,
                                      indemnity_offsets, transferred, "");
        }
        if (transferred == 0) disorder_add_war_pressure(loser, 10);
        if (transferred == 0 || (civs[loser].disorder >= 80 && civs[loser].cohesion <= 3) ||
            (civs[loser].disorder >= 92 && civs[loser].cohesion <= 4)) {
            vassal_make(winner, loser, margin >= 3 ? 18 : 25);
        } else {
            diplomacy_start_truce(winner, loser, 55, margin >= 3 ? 20 : 30);
        }
    } else if (is_valid_civ_id(attacker) && is_valid_civ_id(defender)) {
        diplomacy_record_war_no_winner(attacker, defender, DIP_LAST_WAR_NEGOTIATED_TRUCE);
        diplomacy_start_truce(attacker, defender, 25, 45);
    }
    world_recalculate_territory();
    world_invalidate_region_cache();
}

void war_apply_outcome(int attacker, int defender, WarOutcome outcome, int margin,
                       int loser_casualties, int loser_initial_soldiers) {
    war_apply_outcome_with_result(attacker, defender, outcome, margin,
                                  loser_casualties, loser_initial_soldiers, DIP_LAST_WAR_MILITARY);
}
