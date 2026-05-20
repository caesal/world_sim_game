#include "sim/war_resolution.h"

#include "sim/diplomacy.h"
#include "sim/disorder.h"
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

static int province_borders_owner(int province_id, int owner) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x;
    int y;

    if (province_id < 0 || owner < 0) return 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int d;
            if (world[y][x].province_id != province_id) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0];
                int ny = y + dirs[d][1];
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (world[ny][nx].owner == owner && world[ny][nx].province_id >= 0) return 1;
            }
        }
    }
    return 0;
}

static int province_value_for_winner(int province_id, int winner) {
    RegionSummary province = summarize_city_region(province_id);
    CountrySummary country = summarize_country(winner);
    int score = province.tiles / 2 + province.population / 12 + province.habitability * 4;

    score += province.food * resource_deficit_value(country.food, 5) * 2;
    score += province.water * resource_deficit_value(country.water, 5) * 3;
    score += province.minerals * resource_deficit_value(country.minerals, 5) * 2;
    score += province.wood * resource_deficit_value(country.wood, 5) * 2;
    score += province.stone * resource_deficit_value(country.stone, 5) * 2;
    score += province.money * 3;
    if (cities[province_id].port) score += 18;
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
    int score;

    if (!cities[city_id].alive || cities[city_id].owner != owner) return -1000000;
    score = cities[city_id].population / 2 + province.food * 4 + province.water * 4 +
            province.money * 3 + province.habitability * 4 + province.tiles;
    if (cities[city_id].port) score += 20;
    if (province_borders_owner(city_id, enemy)) score -= 60;
    return score;
}

static void choose_new_capital(int loser, int winner) {
    int i;
    int best = -1;
    int best_score = -1000000;

    for (i = 0; i < city_count; i++) {
        int score;
        if (cities[i].alive && cities[i].owner == loser) cities[i].capital = 0;
        score = capital_candidate_score(i, loser, winner);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0) {
        cities[best].capital = 1;
        civs[loser].capital_city = best;
    } else {
        civs[loser].capital_city = -1;
        civs[loser].alive = 0;
    }
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
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && owner_in_loser_side(loser, cities[i].owner)) count++;
    }
    return count;
}

static int owner_can_cede_province(int loser, int owner) {
    if (vassal_is_direct(loser, owner) && war_owned_province_count(owner) <= 1) return 0;
    return 1;
}

static int winner_capital_distance(int province_id, int winner) {
    int capital = civs[winner].capital_city;
    if (capital < 0 || capital >= city_count || !cities[capital].alive) return 1000000;
    return abs(cities[province_id].x - cities[capital].x) +
           abs(cities[province_id].y - cities[capital].y);
}

static int province_port_distance_to_winner(int province_id, int winner) {
    int best = 1000000;
    int px = cities[province_id].port_x >= 0 ? cities[province_id].port_x : cities[province_id].x;
    int py = cities[province_id].port_y >= 0 ? cities[province_id].port_y : cities[province_id].y;
    int i;
    if (!cities[province_id].port) return best;
    for (i = 0; i < city_count; i++) {
        int wx, wy, distance;
        if (!cities[i].alive || cities[i].owner != winner || !cities[i].port) continue;
        if (!sea_lanes_network_connected(i, province_id)) continue;
        wx = cities[i].port_x >= 0 ? cities[i].port_x : cities[i].x;
        wy = cities[i].port_y >= 0 ? cities[i].port_y : cities[i].y;
        distance = abs(px - wx) + abs(py - wy);
        if (distance < best) best = distance;
    }
    return best;
}

static int pick_side_land_province(int loser, int winner) {
    int i;
    int best = -1;
    int best_distance = 1000000;
    int best_value = -1000000;

    for (i = 0; i < city_count; i++) {
        int owner;
        int distance;
        int value;
        if (!cities[i].alive) continue;
        owner = cities[i].owner;
        if (!owner_in_loser_side(loser, owner) || !owner_can_cede_province(loser, owner)) continue;
        if (!province_borders_owner(i, winner)) continue;
        distance = winner_capital_distance(i, winner);
        value = province_value_for_winner(i, winner);
        if (distance < best_distance || (distance == best_distance && value > best_value)) {
            best_distance = distance;
            best_value = value;
            best = i;
        }
    }
    return best;
}

static int pick_side_port_province(int loser, int winner) {
    int i;
    int best = -1;
    int best_distance = 1000000;
    int best_value = -1000000;
    for (i = 0; i < city_count; i++) {
        int owner;
        int distance;
        int value;
        if (!cities[i].alive || !cities[i].port) continue;
        owner = cities[i].owner;
        if (!owner_in_loser_side(loser, owner) || !owner_can_cede_province(loser, owner)) continue;
        distance = province_port_distance_to_winner(i, winner);
        if (distance >= 1000000) continue;
        value = province_value_for_winner(i, winner);
        if (distance < best_distance || (distance == best_distance && value > best_value)) {
            best_distance = distance;
            best_value = value;
            best = i;
        }
    }
    return best;
}

static int pick_side_cession_province(int loser, int winner) {
    int province_id = pick_side_land_province(loser, winner);
    return province_id >= 0 ? province_id : pick_side_port_province(loser, winner);
}

static int transfer_side_border_provinces(int loser, int winner, int count) {
    int transferred = 0;

    while (transferred < count) {
        int province_id = pick_side_cession_province(loser, winner);
        int province_owner;
        int capital_lost;
        if (province_id < 0) break;
        province_owner = cities[province_id].owner;
        capital_lost = province_id == civs[province_owner].capital_city || cities[province_id].capital;
        cities[province_id].capital = 0;
        if (capital_lost) civs[province_owner].capital_city = -1;
        world_claim_city_region(province_id, winner);
        disorder_add_war_pressure(province_owner, 12);
        transferred++;
        if (capital_lost) handle_capital_loss(province_owner, winner);
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

int war_owned_province_count(int civ_id) {
    int i;
    int count = 0;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == civ_id) count++;
    }
    return count;
}

void war_apply_outcome(int attacker, int defender, WarOutcome outcome, int margin,
                       int loser_casualties, int loser_initial_soldiers) {
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
        diplomacy_record_war_result(winner, loser);
        transferred = transfer_side_border_provinces(loser, winner, cession_count);
        if (transferred == 0) disorder_add_war_pressure(loser, 10);
        if (transferred == 0 || (civs[loser].disorder >= 80 && civs[loser].cohesion <= 3)) {
            vassal_make(winner, loser, margin >= 3 ? 18 : 25);
        } else {
            diplomacy_start_truce(winner, loser, 55, margin >= 3 ? 20 : 30);
        }
    } else if (is_valid_civ_id(attacker) && is_valid_civ_id(defender)) {
        diplomacy_start_truce(attacker, defender, 25, 45);
    }
    world_recalculate_territory();
    world_invalidate_region_cache();
}
