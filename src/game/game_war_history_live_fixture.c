#include "game/game_war_history_live_fixture.h"

#include "core/game_types.h"
#include "io/map_save.h"
#include "sim/diplomacy.h"
#include "sim/regions.h"
#include "sim/war.h"
#include "sim/war_front.h"
#include "sim/war_history.h"
#include "sim/war_internal.h"
#include "sim/war_terminal.h"

#include <stdlib.h>
#include <string.h>

#define LIVE_FIXTURE_ACK "phase2-owned-hwnd"
#define LIVE_FIXTURE_MIN_REGIONS 10
#define LIVE_FIXTURE_MIN_CONTACTS 3

int war_history_live_fixture_status;
int war_history_live_fixture_selected_civ = -1;
int war_history_live_fixture_opponent_civ = -1;
int war_history_live_fixture_requested_count = WAR_HISTORY_CAPACITY;
int war_history_live_fixture_history_count;
int war_history_live_fixture_actual_cession;
int war_history_live_fixture_actual_indemnity;
int war_history_live_fixture_save_reload_ok;
int war_history_live_fixture_active_war_ok;
int war_history_live_fixture_map_version;
uint64_t war_history_live_fixture_revision_before_save;
uint64_t war_history_live_fixture_revision_after_load;
uint64_t war_history_live_fixture_active_serial_before_save;
uint64_t war_history_live_fixture_active_serial_after_load;

static int fixture_requested(void) {
    const char *value = getenv("WORLD_SIM_WAR_HISTORY_GUI_FIXTURE");
    return value && strcmp(value, LIVE_FIXTURE_ACK) == 0;
}

static int fixture_record_count(void) {
    const char *value = getenv("WORLD_SIM_WAR_HISTORY_GUI_COUNT");
    if (!value) return WAR_HISTORY_CAPACITY;
    if (!value[0] || value[1] != '\0' ||
        value[0] < '0' || value[0] > '3') return -1;
    return value[0] - '0';
}

static int owned_region_count(int civ_id) {
    int count = 0;
    int region_id;
    for (region_id = 0; region_id < region_count; region_id++) {
        if (natural_regions[region_id].alive &&
            natural_regions[region_id].owner_civ == civ_id) count++;
    }
    return count;
}

static int claim_to_count(int civ_id, int target) {
    int region_id;
    for (region_id = 0; region_id < region_count &&
         owned_region_count(civ_id) < target; region_id++) {
        if (natural_regions[region_id].alive &&
            natural_regions[region_id].owner_civ < 0) {
            regions_claim_for_civ(region_id, civ_id, -1, 1);
        }
    }
    return owned_region_count(civ_id) >= target;
}

static int land_contact_count(int civ_a, int civ_b) {
    int count = 0;
    int region_id;
    for (region_id = 0; region_id < region_count; region_id++) {
        const NaturalRegion *region = &natural_regions[region_id];
        int neighbor_index;
        if (!region->alive || region->owner_civ != civ_b) continue;
        for (neighbor_index = 0; neighbor_index < region->neighbor_count;
             neighbor_index++) {
            int neighbor = region->neighbors[neighbor_index];
            if (neighbor >= 0 && neighbor < region_count &&
                natural_regions[neighbor].alive &&
                natural_regions[neighbor].owner_civ == civ_a) {
                count++;
                break;
            }
        }
    }
    return count;
}

static int ensure_land_contacts(int civ_a, int civ_b, int target) {
    int region_id;
    for (region_id = 0; region_id < region_count &&
         land_contact_count(civ_a, civ_b) < target; region_id++) {
        const NaturalRegion *region = &natural_regions[region_id];
        int neighbor_index;
        if (!region->alive || region->owner_civ != civ_b) continue;
        for (neighbor_index = 0; neighbor_index < region->neighbor_count;
             neighbor_index++) {
            int neighbor = region->neighbors[neighbor_index];
            if (neighbor < 0 || neighbor >= region_count ||
                !natural_regions[neighbor].alive ||
                natural_regions[neighbor].owner_civ >= 0) continue;
            if (regions_claim_for_civ(neighbor, civ_a, -1, 1)) break;
        }
    }
    return land_contact_count(civ_a, civ_b) >= target;
}

static ActiveWar *active_between(int civ_a, int civ_b) {
    int index;
    for (index = 0; index < MAX_ACTIVE_WARS; index++) {
        ActiveWar *war = &active_wars[index];
        if (!war->active) continue;
        if ((war->attacker == civ_a && war->defender == civ_b) ||
            (war->attacker == civ_b && war->defender == civ_a)) return war;
    }
    return NULL;
}

static ActiveWar *start_fixture_war(int attacker, int defender,
                                    int months_ago) {
    ActiveWar *war;
    int absolute_now = max(0, year) * 12 + clamp(month, 1, 12) - 1;
    if (!war_start_independence(attacker, defender)) return NULL;
    war = active_between(attacker, defender);
    if (!war) return NULL;
    war->start_absolute_month = max(0, absolute_now - max(1, months_ago));
    war->initial_national_a = max(1000, war->initial_national_a);
    war->initial_national_b = max(1000, war->initial_national_b);
    war->initial_soldiers_a = max(1000, war->initial_soldiers_a);
    war->initial_soldiers_b = max(1000, war->initial_soldiers_b);
    war->soldiers_a = max(600, war->soldiers_a);
    war->soldiers_b = max(600, war->soldiers_b);
    return war;
}

static int history_shape_ok(int selected, int opponent, int requested_count,
                            WarHistory *history) {
    int record_index;
    if (!war_history_copy_for_civ(selected, civs[selected].uid, history) ||
        history->count != requested_count ||
        history->records[0].result != DIP_LAST_WAR_MILITARY ||
        history->records[0].beneficiary_uid != civs[selected].uid ||
        history->records[0].transferred_regions <= 0 ||
        history->records[0].indemnity_paid <= 0) return 0;
    if (requested_count >= 2 &&
        history->records[1].result != DIP_LAST_WAR_OFFENSIVE_HALTED) return 0;
    if (requested_count >= 3 &&
        history->records[2].result != DIP_LAST_WAR_NEGOTIATED_TRUCE) return 0;
    for (record_index = 0; record_index < requested_count; record_index++) {
        if (history->records[record_index].local.uid != civs[selected].uid ||
            history->records[record_index].opponent.uid != civs[opponent].uid) {
            return 0;
        }
    }
    return 1;
}

static int build_terminal_fixture(int selected, int opponent,
                                  int requested_count,
                                  WarHistory *history) {
    ActiveWar *war;
    if (!claim_to_count(selected, LIVE_FIXTURE_MIN_REGIONS) ||
        !claim_to_count(opponent, LIVE_FIXTURE_MIN_REGIONS) ||
        !ensure_land_contacts(selected, opponent, LIVE_FIXTURE_MIN_CONTACTS)) {
        return 0;
    }
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    civs[selected].disorder = 0;
    civs[selected].cohesion = max(civs[selected].cohesion, 8);
    civs[opponent].disorder = 0;
    civs[opponent].cohesion = max(civs[opponent].cohesion, 8);
    civs[opponent].treasury = max(civs[opponent].treasury, 1000000);
    civs[opponent].treasury_cap = max(civs[opponent].treasury_cap, 1000000);

    if (requested_count >= 3) {
        war = start_fixture_war(selected, opponent, 42);
        if (!war) return 0;
        war->casualties_a = 12000;
        war->casualties_b = 18000;
        war_terminal_finish(war, WAR_OUTCOME_STALEMATE, 0,
                            DIP_LAST_WAR_NEGOTIATED_TRUCE);
    }

    if (requested_count >= 2) {
        war = start_fixture_war(selected, opponent, 27);
        if (!war) return 0;
        war->casualties_a = 24000;
        war->casualties_b = 39000;
        war_terminal_offensive_halted(war, 0, 25, 45);
    }

    war = start_fixture_war(selected, opponent, 16);
    if (!war) return 0;
    war->casualties_a = 47000;
    war->casualties_b = 88000;
    war->initial_soldiers_b = 1000;
    war_terminal_finish(war, WAR_OUTCOME_ATTACKER_WIN, 2,
                        DIP_LAST_WAR_MILITARY);
    if (!history_shape_ok(selected, opponent, requested_count, history)) {
        return 0;
    }
    war_history_live_fixture_actual_cession =
        history->records[0].transferred_regions;
    war_history_live_fixture_actual_indemnity =
        history->records[0].indemnity_paid;

    war = start_fixture_war(selected, opponent, 5);
    if (!war) return 0;
    war->casualties_a = 13579;
    war->casualties_b = 24680;
    war_history_live_fixture_active_serial_before_save = war->war_serial;
    return 1;
}

static int full_save_reload(HWND hwnd, int selected, int opponent,
                            const WarHistory *before) {
    WarHistory after = {0};
    ActiveWar reloaded;
    war_history_live_fixture_revision_before_save = before->revision;
    if (!save_current_map(hwnd)) return 0;
    war_reset();
    if (!load_map_from_file(hwnd)) return 0;
    if (!war_history_copy_for_civ(selected, civs[selected].uid, &after) ||
        memcmp(before, &after, sizeof(after)) != 0) return 0;
    reloaded = war_state_between(selected, opponent);
    war_history_live_fixture_revision_after_load = after.revision;
    war_history_live_fixture_active_serial_after_load = reloaded.war_serial;
    war_history_live_fixture_active_war_ok =
        reloaded.active && reloaded.war_serial ==
        war_history_live_fixture_active_serial_before_save;
    return war_history_live_fixture_active_war_ok;
}

void game_war_history_live_fixture_after_worldgen(HWND hwnd) {
    static int attempted;
    WarHistory history = {0};
    int selected = -1;
    int opponent = -1;
    int requested_count;
    int civ_id;
    if (attempted || !fixture_requested()) return;
    attempted = 1;
    war_history_live_fixture_status = -1;
    war_history_live_fixture_map_version = map_save_current_version();
    requested_count = fixture_record_count();
    war_history_live_fixture_requested_count = requested_count;
    if (requested_count < 0) {
        war_history_live_fixture_status = -4;
        return;
    }
    if (!world_generated || civ_count < 26 || map_save_current_version() != 21) return;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        if (!civs[civ_id].alive) continue;
        if (selected < 0) selected = civ_id;
        else { opponent = civ_id; break; }
    }
    if (selected < 0 || opponent < 0) return;
    war_history_live_fixture_selected_civ = selected;
    war_history_live_fixture_opponent_civ = opponent;
    if (requested_count == 0) {
        if (!war_history_copy_for_civ(selected, civs[selected].uid, &history) ||
            history.count != 0) return;
        war_history_live_fixture_history_count = 0;
        war_history_live_fixture_status = 1;
        return;
    }
    war_history_live_fixture_status = -2;
    if (!build_terminal_fixture(selected, opponent, requested_count, &history)) return;
    war_history_live_fixture_history_count = history.count;
    war_history_live_fixture_status = -3;
    if (!full_save_reload(hwnd, selected, opponent, &history)) return;
    war_history_live_fixture_save_reload_ok = 1;
    war_history_live_fixture_status = 1;
}
