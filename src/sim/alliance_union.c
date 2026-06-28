#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_notifications.h"
#include "core/game_state.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"

#include <stdio.h>
#include <string.h>

static int alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
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

static int first_owned_region(int civ_id) {
    int i;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) return i;
    }
    return -1;
}

static void copy_founder_traits(int new_civ, const Civilization *founder) {
    int name_id;
    civs[new_civ].alive = 1;
    civs[new_civ].symbol = (char)('A' + (new_civ % 26));
    civs[new_civ].aggression = founder->aggression;
    civs[new_civ].expansion = founder->expansion;
    civs[new_civ].defense = founder->defense;
    civs[new_civ].culture = founder->culture;
    civs[new_civ].governance = founder->governance;
    civs[new_civ].cohesion = founder->cohesion;
    civs[new_civ].production = founder->production;
    civs[new_civ].military = founder->military;
    civs[new_civ].commerce = founder->commerce;
    civs[new_civ].logistics = founder->logistics;
    civs[new_civ].innovation = founder->innovation;
    civs[new_civ].adaptation = founder->adaptation;
    civs[new_civ].tech_stage = founder->tech_stage;
    civs[new_civ].tech_progress = founder->tech_progress;
    civs[new_civ].heritage = civilization_heritage_or_default(founder->heritage);
    name_id = civilization_pick_unused_name_id_for_heritage(civs[new_civ].heritage);
    civilization_assign_generated_name_for_heritage(&civs[new_civ], civs[new_civ].heritage, name_id);
}

static int member_mask_contains(const unsigned char *mask, int civ_id) {
    return civ_id >= 0 && civ_id < MAX_CIVS && mask[civ_id];
}

static void transfer_owned_world(int new_civ, const unsigned char *member_mask) {
    int x, y, i;
    for (i = 0; i < region_count; i++) {
        if (member_mask_contains(member_mask, natural_regions[i].owner_civ)) natural_regions[i].owner_civ = new_civ;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (member_mask_contains(member_mask, world[y][x].owner)) world[y][x].owner = new_civ;
        }
    }
}

static int transfer_cities(int new_civ, const int *members, int count, int founder_capital) {
    int i, m, capital = -1;
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive) continue;
        for (m = 0; m < count; m++) {
            if (cities[i].owner != members[m]) continue;
            cities[i].owner = new_civ;
            cities[i].capital = 0;
            if (i == founder_capital) capital = i;
        }
    }
    if (capital < 0) {
        for (i = 0; i < city_count; i++) {
            if (cities[i].alive && cities[i].owner == new_civ) { capital = i; break; }
        }
    }
    if (capital >= 0) cities[capital].capital = 1;
    return capital;
}

static void copy_founder_diplomacy(int founder, int new_civ, const unsigned char *member_mask) {
    int other;
    for (other = 0; other < civ_count; other++) {
        DiplomacyRelation a;
        DiplomacyRelation b;
        if (other == new_civ || member_mask_contains(member_mask, other) || !civs[other].alive) continue;
        a = diplomacy_relation(founder, other);
        b = diplomacy_relation(other, founder);
        if (a.state == DIPLOMACY_VASSAL && a.overlord == founder) a.overlord = new_civ;
        if (b.state == DIPLOMACY_VASSAL && b.overlord == founder) b.overlord = new_civ;
        diplomacy_restore_relation(new_civ, other, a);
        diplomacy_restore_relation(other, new_civ, b);
    }
}

static void retire_members(const int *members, int count, int founder, int new_civ) {
    int i;
    war_transfer_civ_identity(founder, new_civ);
    for (i = 0; i < count; i++) {
        if (members[i] != founder) war_end_direct_for_civ(members[i]);
    }
    for (i = 0; i < count; i++) {
        diplomacy_clear_civ(members[i]);
        civs[members[i]].alive = 0;
        civs[members[i]].population = 0;
        civs[members[i]].territory = 0;
        civs[members[i]].capital_city = -1;
        civs[members[i]].treasury = 0;
        civs[members[i]].treasury_pending_surplus = 0;
    }
}

static void finish_world_refresh(int new_civ, const int *members, int count) {
    int i;
    for (i = 0; i < count; i++) world_mark_province_partition_dirty(members[i]);
    world_mark_province_partition_dirty(new_civ);
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
    for (i = 0; i < count; i++) {
        if (members[i] >= 0 && members[i] < MAX_CIVS) state->civ_alliance[members[i]] = -1;
    }
    state->records[alliance_id].active = 0;
    state->records[alliance_id].member_count = 0;
    alliance_power_cache_reset();
}

static void notify_union(const AllianceRecord *record, int founder, int new_civ, int count) {
    char en[GAME_NOTIFICATION_TEXT], zh[GAME_NOTIFICATION_TEXT];
    snprintf(en, sizeof(en), "%s united under %s as %s; %d members merged.",
             record->name_en, civilization_display_name_for_language(founder, 0),
             civilization_display_name_for_language(new_civ, 0), count);
    snprintf(zh, sizeof(zh), "%s完成联合，由%s领导，建立%s；合并%d个成员。",
             record->name_zh, civilization_display_name_for_language(founder, 1),
             civilization_display_name_for_language(new_civ, 1), count);
    game_notifications_push(en, zh);
}

int alliance_union_try(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceRecord *record;
    Civilization founder_copy;
    int members[MAX_CIVS], count, latest_join, founder, new_civ, i, treasury = 0, treasury_cap = 0;
    int seed_region, capital;
    unsigned char member_mask[MAX_CIVS] = {0};
    if (!state || alliance_id < 0 || alliance_id >= state->next_id || alliance_id >= ALLIANCE_MAX) return 0;
    record = &state->records[alliance_id];
    if (!record->active) return 0;
    count = collect_members(record, members, &latest_join);
    if (count < 2 || year - latest_join < alliance_union_required_years_for_type(state->alliance_type[alliance_id]))
        return 0;
    founder = record->founder_civ_id;
    if (!alive_civ(founder) || !alliance_is_formal_member(alliance_id, founder)) return 0;
    new_civ = civilization_allocate_slot(1);
    if (new_civ < 0) return 0;
    founder_copy = civs[founder];
    for (i = 0; i < count; i++) {
        member_mask[members[i]] = 1;
        treasury += max(0, civs[members[i]].treasury);
        treasury_cap += max(0, civs[members[i]].treasury_cap);
    }
    copy_founder_traits(new_civ, &founder_copy);
    transfer_owned_world(new_civ, member_mask);
    capital = transfer_cities(new_civ, members, count, founder_copy.capital_city);
    civs[new_civ].capital_city = capital;
    civs[new_civ].treasury = treasury;
    civs[new_civ].treasury_cap = max(treasury, treasury_cap);
    civs[new_civ].disorder = 0;
    civs[new_civ].disorder_resource = civs[new_civ].disorder_plague = 0;
    civs[new_civ].disorder_migration = civs[new_civ].disorder_stability = 0;
    seed_region = capital >= 0 ? regions_region_for_city(capital) : first_owned_region(new_civ);
    copy_founder_diplomacy(founder, new_civ, member_mask);
    retire_members(members, count, founder, new_civ);
    civs[new_civ].color = civilization_pick_distinct_color(new_civ, 0, founder, seed_region);
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_UNION_FORMED, new_civ, founder, -1, ALLIANCE_REJECT_NONE);
    event_log_push_structured(EVENT_TYPE_CIV_CREATED, EVENT_SEVERITY_INFO,
                              new_civ, founder, -1, capital, alliance_id, count, "");
    notify_union(record, founder, new_civ, count);
    deactivate_alliance(alliance_id, members, count);
    finish_world_refresh(new_civ, members, count);
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
