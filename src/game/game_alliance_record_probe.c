#include "game/game_alliance_record_probe.h"

#include "core/game_state.h"
#include "core/game_notifications.h"
#include "game/game_worldgen.h"
#include "io/map_save.h"
#include "io/map_save_state.h"
#include "sim/alliance.h"
#include "sim/alliance_contact.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_borders.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_resolution.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <string.h>

static void init_record_probe_civ(int id, const char *name) {
    memset(&civs[id], 0, sizeof(civs[id]));
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].capital_city = id;
    civs[id].color = COLOR32_RGB(90 + id * 24, 120 + id * 17, 170 + id * 9);
    civs[id].treasury = 100 + id;
}

static void claim_contact_probe_tile(int civ_id, int x, int y) {
    world[y][x].geography = GEO_PLAIN;
    world[y][x].climate = CLIMATE_CONTINENTAL;
    world[y][x].owner = civ_id;
    world[y][x].province_id = civ_id;
    world[y][x].region_id = civ_id;
}

static void init_union_region_city(int id, int owner, int x) {
    NaturalRegion *region = &natural_regions[id];
    City *city = &cities[id];
    memset(region, 0, sizeof(*region));
    memset(city, 0, sizeof(*city));
    region->id = id;
    region->alive = 1;
    region->owner_civ = owner;
    region->city_id = id;
    region->tile_count = 1;
    city->alive = 1;
    city->owner = owner;
    city->x = x;
    city->y = 3;
    city->population = 1000 + id * 100;
    city->radius = 2;
    city->capital = civs[owner].capital_city == id;
    world[3][x].geography = GEO_PLAIN;
    world[3][x].climate = CLIMATE_CONTINENTAL;
    world[3][x].owner = owner;
    world[3][x].province_id = id;
    world[3][x].region_id = id;
    population_init_city(id, city->population);
}

static void add_record_probe_neighbor(int a, int b) {
    NaturalRegion *ra = &natural_regions[a];
    NaturalRegion *rb = &natural_regions[b];
    if (ra->neighbor_count < MAX_REGION_NEIGHBORS) ra->neighbors[ra->neighbor_count++] = b;
    if (rb->neighbor_count < MAX_REGION_NEIGHBORS) rb->neighbors[rb->neighbor_count++] = a;
}

static int record_probe_owned_regions(int owner) {
    int i, count = 0;
    for (i = 0; i < region_count; i++)
        if (natural_regions[i].alive && natural_regions[i].owner_civ == owner) count++;
    return count;
}

static int record_probe_event_count(EventLogType type) {
    int i, count = 0;
    for (i = 0; i < event_log_count; i++)
        if (event_log_get_type(i) == type) count++;
    return count;
}

static int reset_war_settlement_fixture(int allied, int extra_member, int severe) {
    int i, alliance_id = -1;
    set_active_map_size(MAP_SIZE_SMALL);
    alliance_reset(); diplomacy_reset(); war_reset(); plague_reset(); event_log_clear();
    game_clear_world_tiles();
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    civ_count = extra_member ? 4 : 3;
    city_count = extra_member ? 22 : 21;
    region_count = city_count;
    world_generated = 1;
    year = 1000; month = 1;
    for (i = 0; i < civ_count; i++) {
        init_record_probe_civ(i, i == 0 ? "Winner" : i == 1 ? "Loser" :
                              i == 2 ? "Ally A" : "Ally B");
        civs[i].capital_city = i == 0 ? 0 : i == 1 ? 10 : i == 2 ? 20 : 21;
        civs[i].cohesion = i == 1 && severe ? 3 : 7;
        civs[i].disorder = i == 1 && severe ? 85 : 20;
        civs[i].treasury = i == 1 ? 1000 : 800;
        civs[i].treasury_cap = 1000;
    }
    for (i = 0; i < 10; i++) {
        init_union_region_city(i, 0, 3 + i);
        init_union_region_city(10 + i, 1, 20 + i);
        add_record_probe_neighbor(i, 10 + i);
    }
    init_union_region_city(20, 2, 40);
    if (extra_member) init_union_region_city(21, 3, 42);
    terrain_stats_invalidate_cache();
    world_recalculate_territory();
    population_sync_all();
    diplomacy_borders_mark_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    if (allied) {
        alliance_id = alliance_debug_create_pair(1, 2, 80);
        if (extra_member) alliance_debug_add_member(alliance_id, 3, 80);
    }
    event_log_clear();
    return alliance_id;
}

static void reset_contact_probe_fixture(void) {
    int i;
    set_active_map_size(MAP_SIZE_SMALL);
    alliance_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    event_log_clear();
    game_clear_world_tiles();
    civ_count = 3;
    city_count = 0;
    region_count = 0;
    world_generated = 1;
    year = 901;
    month = 1;
    for (i = 0; i < civ_count; i++) init_record_probe_civ(i, i == 0 ? "Formal A" :
                                                            i == 1 ? "Formal B" : "Candidate C");
    claim_contact_probe_tile(0, 3, 3);
    claim_contact_probe_tile(2, 4, 3);
    claim_contact_probe_tile(1, 20, 20);
    diplomacy_borders_mark_dirty();
    diplomacy_mark_contacts_dirty();
}

static int case_alliance_record_roundtrip(FILE *summary) {
    AllianceSaveState *state;
    signed char votes[MAX_CIVS];
    FILE *file;
    int id;
    int i;
    int ok = 1;

    alliance_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    event_log_clear();
    civ_count = 3;
    world_generated = 1;
    year = 900;
    for (i = 0; i < civ_count; i++) {
        memset(&civs[i], 0, sizeof(civs[i]));
        civs[i].alive = 1;
        civs[i].capital_city = i;
        civs[i].treasury = 100 + i;
    }
    id = alliance_debug_create_pair(0, 1, 0);
    for (i = 0; i < MAX_CIVS; i++) votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    votes[0] = ALLIANCE_MEMBER_VOTE_YES;
    votes[1] = ALLIANCE_MEMBER_VOTE_NO;
    alliance_record_candidate(id, 2, ALLIANCE_CANDIDATE_JOIN,
                              ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE, 898, 75,
                              ALLIANCE_CANDIDATE_ACTIVE, ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_vote(id, ALLIANCE_VOTE_JOIN, 2, -1, votes, 1, 1, 0,
                         ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_history(id, ALLIANCE_HISTORY_MEMBER_REMOVED, 2, -1,
                            ALLIANCE_VOTE_REMOVAL, ALLIANCE_REJECT_NONE);
    file = tmpfile();
    ok &= file != NULL;
    if (file) {
        ok &= map_save_write_dynamic_state(file);
        alliance_reset();
        rewind(file);
        ok &= map_save_read_dynamic_state(file, map_save_current_version()) > 0;
        fclose(file);
    }
    state = alliance_internal_state();
    ok &= state->candidate_count[id] >= 1;
    ok &= state->vote_count[id] >= 1;
    ok &= state->history_count[id] >= 3;
    ok &= state->candidates[id][0].status == ALLIANCE_CANDIDATE_ACTIVE;
    ok &= state->candidates[id][0].rejection_reason == ALLIANCE_REJECT_VOTE_FAILED;
    ok &= state->votes[id][0].active && state->votes[id][0].member_votes[0] == ALLIANCE_MEMBER_VOTE_YES;
    ok &= state->votes[id][0].member_votes[1] == ALLIANCE_MEMBER_VOTE_NO;
    fprintf(summary,
            "case=alliance_record_save_roundtrip ok=%d alliance=%d candidates=%d votes=%d history=%d yes=%d no=%d reason=%d\n",
            ok, id, state->candidate_count[id], state->vote_count[id],
            state->history_count[id], state->votes[id][0].yes_count,
            state->votes[id][0].no_count, state->votes[id][0].rejection_reason);
    return ok;
}

static int case_alliance_diplomatic_contact(FILE *summary) {
    int id;
    int ok = 1;
    DiplomacyRelation indirect;
    reset_contact_probe_fixture();
    id = alliance_debug_create_pair(0, 1, 0);
    diplomacy_borders_mark_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    indirect = diplomacy_relation(2, 1);
    ok &= id >= 0;
    ok &= diplomacy_direct_contact_kind(2, 0) == DIP_CONTACT_LAND_BORDER;
    ok &= diplomacy_direct_contact_kind(2, 1) == DIP_CONTACT_NONE;
    ok &= alliance_diplomatic_contact_source(2, id, 0) == ALLIANCE_DIP_CONTACT_DIRECT;
    ok &= alliance_diplomatic_contact_source(2, id, 1) == ALLIANCE_DIP_CONTACT_ALLIANCE;
    ok &= alliance_diplomatic_contact_between(2, 1) == 1;
    ok &= indirect.state == DIPLOMACY_PEACE && indirect.contact_kind == DIP_CONTACT_NONE;
    fprintf(summary,
            "case=alliance_diplomatic_contact ok=%d alliance=%d direct_20=%d direct_21=%d source_20=%d source_21=%d indirect_state=%d indirect_kind=%d\n",
            ok, id, diplomacy_direct_contact_kind(2, 0), diplomacy_direct_contact_kind(2, 1),
            alliance_diplomatic_contact_source(2, id, 0),
            alliance_diplomatic_contact_source(2, id, 1),
            indirect.state, indirect.contact_kind);
    return ok;
}

static int case_alliance_retryable_vote_records(FILE *summary) {
    AllianceSaveState *state;
    signed char votes[MAX_CIVS];
    int id, ok = 1, i;
    reset_contact_probe_fixture();
    id = alliance_debug_create_pair(0, 1, 0);
    for (i = 0; i < MAX_CIVS; i++) votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    votes[0] = ALLIANCE_MEMBER_VOTE_YES;
    votes[1] = ALLIANCE_MEMBER_VOTE_NO;
    alliance_record_candidate(id, 2, ALLIANCE_CANDIDATE_JOIN,
                              ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE, year, 100,
                              ALLIANCE_CANDIDATE_ACTIVE, ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_vote(id, ALLIANCE_VOTE_JOIN, 2, -1, votes, 1, 1, 0,
                         ALLIANCE_REJECT_VOTE_FAILED);
    state = alliance_internal_state();
    ok &= id >= 0 && state->candidate_count[id] >= 1 && state->vote_count[id] >= 1;
    ok &= state->candidates[id][0].status == ALLIANCE_CANDIDATE_ACTIVE;
    ok &= state->candidates[id][0].qualification_progress == 100;
    ok &= state->votes[id][0].passed == 0;
    ok &= state->votes[id][0].rejection_reason == ALLIANCE_REJECT_VOTE_FAILED;
    ok &= state->votes[id][0].member_votes[0] == ALLIANCE_MEMBER_VOTE_YES;
    ok &= state->votes[id][0].member_votes[1] == ALLIANCE_MEMBER_VOTE_NO;
    fprintf(summary,
            "case=alliance_retryable_vote_records ok=%d alliance=%d candidate_status=%d progress=%d passed=%d reason=%d vote0=%d vote1=%d\n",
            ok, id, state->candidates[id][0].status,
            state->candidates[id][0].qualification_progress,
            state->votes[id][0].passed, state->votes[id][0].rejection_reason,
            state->votes[id][0].member_votes[0], state->votes[id][0].member_votes[1]);
    return ok;
}

static int case_alliance_stale_join_application_closed(FILE *summary) {
    AllianceSaveState *state;
    int a, b, ok = 1, blocked = 0, history = 0, i;
    set_active_map_size(MAP_SIZE_SMALL);
    alliance_reset(); diplomacy_reset(); war_reset(); plague_reset(); event_log_clear();
    civ_count = 5; world_generated = 1; year = 940; month = 1;
    for (i = 0; i < civ_count; i++) init_record_probe_civ(i, i == 4 ? "Stale Candidate" : "Member");
    a = alliance_debug_create_pair(0, 1, 80);
    b = alliance_debug_create_pair(2, 3, 80);
    alliance_record_candidate(a, 4, ALLIANCE_CANDIDATE_JOIN,
                              ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE, year - 12, 40,
                              ALLIANCE_CANDIDATE_ACTIVE, ALLIANCE_REJECT_NONE);
    alliance_debug_set_join_years(4, a, 12);
    ok &= a >= 0 && b >= 0 && alliance_debug_add_member(b, 4, 80);
    alliance_update_year();
    state = alliance_internal_state();
    ok &= alliance_for_civ(4) == b && state->join_years[4][a] == 0;
    for (i = 0; i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        AllianceCandidateRecord *c = &state->candidates[a][i];
        if (c->active && c->civ_id == 4 && c->type == ALLIANCE_CANDIDATE_JOIN &&
            c->status == ALLIANCE_CANDIDATE_BLOCKED &&
            c->rejection_reason == ALLIANCE_REJECT_JOINED_ANOTHER_ALLIANCE) blocked = 1;
    }
    for (i = 0; i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        AllianceHistoryRecord *h = &state->history[a][i];
        if (h->active && h->civ_id == 4 &&
            h->rejection_reason == ALLIANCE_REJECT_JOINED_ANOTHER_ALLIANCE) history = 1;
    }
    ok &= blocked && history;
    fprintf(summary,
            "case=alliance_stale_join_application_closed ok=%d alliance_a=%d alliance_b=%d candidate_alliance=%d blocked=%d history=%d join_years=%d reason=%d\n",
            ok, a, b, alliance_for_civ(4), blocked, history,
            state->join_years[4][a], ALLIANCE_REJECT_JOINED_ANOTHER_ALLIANCE);
    return ok;
}

static int case_alliance_union_vote_absorption(FILE *summary) {
    AllianceSaveState *state;
    int id, ok = 1, i, vote_history = 0, absorbed_history = 0, event_count;
    set_active_map_size(MAP_SIZE_SMALL);
    alliance_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    event_log_clear();
    game_clear_world_tiles();
    civ_count = 3;
    city_count = 4;
    region_count = 4;
    world_generated = 1;
    year = 900;
    month = 1;
    for (i = 0; i < civ_count; i++) {
        init_record_probe_civ(i, i == 0 ? "Founder" : i == 1 ? "Member" : "Neighbor");
        civs[i].symbol = (char)('A' + i);
        civs[i].military = 5 + i;
        civs[i].governance = 6;
        civs[i].treasury = 1000 + i * 100;
        civs[i].treasury_cap = 2000;
    }
    init_union_region_city(0, 0, 4);
    init_union_region_city(1, 0, 5);
    init_union_region_city(2, 1, 9);
    init_union_region_city(3, 2, 12);
    cities[0].population = 10000; population_init_city(0, 10000);
    cities[1].population = 10000; population_init_city(1, 10000);
    cities[2].population = 100; population_init_city(2, 100);
    world_recalculate_territory();
    id = alliance_debug_create_pair(0, 1, 0);
    war_start(1, 2);
    year = 1700;
    ok &= id >= 0;
    alliance_update_year();
    state = alliance_internal_state();
    for (i = 0; i < state->history_count[id] && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        if (state->history[id][i].active &&
            state->history[id][i].event_type == ALLIANCE_HISTORY_UNION_VOTE_PASSED) vote_history = 1;
        if (state->history[id][i].active &&
            state->history[id][i].event_type == ALLIANCE_HISTORY_UNION_ABSORBED) absorbed_history = 1;
    }
    event_count = record_probe_event_count(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION);
    ok &= civ_count == 3 && civs[0].alive && !civs[1].alive && civs[2].alive;
    ok &= cities[0].owner == 0 && cities[1].owner == 0 && cities[2].owner == 0 && cities[3].owner == 2;
    ok &= natural_regions[0].owner_civ == 0 && natural_regions[1].owner_civ == 0 &&
          natural_regions[2].owner_civ == 0;
    ok &= civs[0].capital_city == 0 && civs[0].treasury == 2100;
    ok &= !war_active_between(1, 2) && !war_active_between(0, 2);
    ok &= state->records[id].active == 0 && alliance_for_civ(0) < 0 && alliance_for_civ(1) < 0;
    ok &= vote_history && absorbed_history && event_count > 0 && game_notifications_count() > 0;
    fprintf(summary,
            "case=alliance_union_vote_absorption ok=%d alliance=%d civ_count=%d proposer_alive=%d absorbed_alive=%d owners=%d/%d/%d/%d treasury=%d external_war=%d inherited_war=%d vote_history=%d absorbed_history=%d events=%d notifications=%d\n",
            ok, id, civ_count, civs[0].alive, civs[1].alive, cities[0].owner,
            cities[1].owner, cities[2].owner, cities[3].owner, civs[0].treasury,
            war_active_between(1, 2), war_active_between(0, 2), vote_history,
            absorbed_history, event_count, game_notifications_count());
    return ok;
}

static int case_war_result_relation_scores(FILE *summary) {
    DiplomacyRelation win_rel, lose_rel;
    int ok = 1;
    reset_war_settlement_fixture(0, 0, 0);
    diplomacy_record_war_result_kind(0, 1, DIP_LAST_WAR_MILITARY);
    win_rel = diplomacy_relation(0, 1);
    lose_rel = diplomacy_relation(1, 0);
    ok &= win_rel.relation_score == -25 && lose_rel.relation_score == -65;
    diplomacy_reset();
    diplomacy_record_war_result_kind(0, 1, DIP_LAST_WAR_SURRENDER);
    win_rel = diplomacy_relation(0, 1);
    lose_rel = diplomacy_relation(1, 0);
    ok &= win_rel.relation_score == -35 && lose_rel.relation_score == -80;
    diplomacy_reset();
    diplomacy_record_war_result_kind(0, 1, DIP_LAST_WAR_DECISIVE);
    win_rel = diplomacy_relation(0, 1);
    lose_rel = diplomacy_relation(1, 0);
    ok &= win_rel.relation_score == -35 && lose_rel.relation_score == -80;
    fprintf(summary,
            "case=war_result_relation_scores ok=%d military=%d/%d surrender=%d/%d decisive=%d/%d\n",
            ok, -25, -65, -35, -80, win_rel.relation_score, lose_rel.relation_score);
    return ok;
}

static int case_war_settlement_alliance_exit(FILE *summary) {
    AllianceSaveState *state;
    AllianceCommandResult player_override;
    int id, ok = 1, no_before_regions, no_after_regions, no_before_treasury, no_after_treasury;
    int forced_before_regions, forced_after_regions, forced_before_treasury, forced_after_treasury;
    int no_indemnity_before, no_indemnity_after, forced_indemnity_before, forced_indemnity_after;
    int cooldown_before, cooldown_after;
    int forced_active, forced_left, forced_vassal;
    int rejoin_alliance, forced_event_ok = 0;
    EventLogEntry forced_event;
    char en[256] = "", zh[256] = "";
    reset_war_settlement_fixture(0, 0, 0);
    no_before_regions = record_probe_owned_regions(1);
    no_before_treasury = civs[1].treasury;
    no_indemnity_before = record_probe_event_count(EVENT_TYPE_TREASURY_INDEMNITY);
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 2, 1000, 100, DIP_LAST_WAR_MILITARY);
    no_after_regions = record_probe_owned_regions(1);
    no_after_treasury = civs[1].treasury;
    no_indemnity_after = record_probe_event_count(EVENT_TYPE_TREASURY_INDEMNITY);
    ok &= no_after_regions < no_before_regions && no_after_treasury < no_before_treasury;
    ok &= no_indemnity_after > no_indemnity_before && vassal_overlord(1) < 0;

    id = reset_war_settlement_fixture(1, 1, 0);
    forced_before_regions = record_probe_owned_regions(1);
    forced_before_treasury = civs[1].treasury;
    forced_indemnity_before = record_probe_event_count(EVENT_TYPE_TREASURY_INDEMNITY);
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 2, 1000, 100, DIP_LAST_WAR_MILITARY);
    state = alliance_internal_state();
    forced_after_regions = record_probe_owned_regions(1);
    forced_after_treasury = civs[1].treasury;
    forced_indemnity_after = record_probe_event_count(EVENT_TYPE_TREASURY_INDEMNITY);
    for (int i = 0; i < event_log_count; i++) {
        if (!event_log_get_entry(i, &forced_event) || forced_event.type != EVENT_TYPE_WAR_FORCED_ALLIANCE_EXIT) continue;
        event_log_format_entry_data(&forced_event, 0, en, sizeof(en));
        event_log_format_entry_data(&forced_event, 1, zh, sizeof(zh));
        forced_event_ok = forced_event.civ_id == 1 && forced_event.target_id == 0 && forced_event.param_a == id &&
                          forced_event.param_b != 0 && strchr(forced_event.raw_message, '\t') &&
                          strstr(en, "Loser") && strstr(en, "Winner") && !strstr(en, "Unknown country") &&
                          strstr(zh, "Loser") && strstr(zh, "Winner") && !strstr(zh, "未知国家");
    }
    cooldown_before = state->kicked_cooldown[id][1];
    forced_active = state->records[id].active;
    forced_left = alliance_for_civ(1) < 0;
    forced_vassal = vassal_overlord(1);
    player_override = alliance_player_form_or_join(1, 2);
    cooldown_after = state->kicked_cooldown[id][1];
    rejoin_alliance = alliance_for_civ(1);
    ok &= id >= 0 && forced_active && forced_left;
    ok &= forced_after_regions == forced_before_regions && forced_after_treasury == forced_before_treasury;
    ok &= forced_indemnity_after == forced_indemnity_before && forced_vassal < 0;
    ok &= forced_event_ok;
    ok &= cooldown_before == 100 && player_override == ALLIANCE_CMD_OK;
    ok &= cooldown_after == 0 && rejoin_alliance == id;
    fprintf(summary, "case=war_forced_alliance_exit_event ok=%d en=%s zh=%s\n", forced_event_ok, en, zh);
    fprintf(summary,
            "case=war_settlement_alliance_exit ok=%d no_alliance_regions=%d/%d treasury=%d/%d indemnity=%d/%d forced_regions=%d/%d forced_treasury=%d/%d forced_active=%d forced_left=%d forced_event=%d cooldown=%d/%d player_override=%d rejoin_alliance=%d id=%d vassal=%d\n",
            ok, no_before_regions, no_after_regions, no_before_treasury, no_after_treasury,
            no_indemnity_before, no_indemnity_after, forced_before_regions, forced_after_regions,
            forced_before_treasury, forced_after_treasury, forced_active, forced_left, forced_event_ok,
            cooldown_before, cooldown_after, player_override, rejoin_alliance, id, forced_vassal);
    return ok;
}

static int case_war_settlement_edge_cases(FILE *summary) {
    AllianceSaveState *state;
    int id, ok = 1, disband_history = 0, removed_history = 0, i;
    int two_member_active, two_member_cooldown;
    id = reset_war_settlement_fixture(1, 0, 0);
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 2, 1000, 100, DIP_LAST_WAR_MILITARY);
    state = alliance_internal_state();
    ok &= id >= 0 && !state->records[id].active && alliance_for_civ(1) < 0 &&
          alliance_for_civ(2) < 0 && state->kicked_cooldown[id][1] == 100;
    two_member_active = state->records[id].active;
    two_member_cooldown = state->kicked_cooldown[id][1];
    for (i = 0; i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        AllianceHistoryRecord *h = &state->history[id][i];
        if (h->active && h->event_type == ALLIANCE_HISTORY_DISSOLVED) disband_history = 1;
        if (h->active && h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT && h->civ_id == 1)
            removed_history = 1;
    }
    ok &= disband_history && removed_history && vassal_overlord(1) < 0;

    id = reset_war_settlement_fixture(1, 1, 1);
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 3, 1000, 100, DIP_LAST_WAR_DECISIVE);
    state = alliance_internal_state();
    ok &= id >= 0 && alliance_for_civ(1) < 0 && state->kicked_cooldown[id][1] == 100;
    ok &= vassal_overlord(1) == 0;
    fprintf(summary,
            "case=war_settlement_edge_cases ok=%d two_member_active=%d cooldown=%d removed_history=%d disband_history=%d severe_vassal=%d\n",
            ok, two_member_active, two_member_cooldown,
            removed_history, disband_history, vassal_overlord(1));
    return ok;
}

int run_alliance_record_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_alliance_record_roundtrip(summary);
    ok &= case_alliance_diplomatic_contact(summary);
    ok &= case_alliance_retryable_vote_records(summary);
    ok &= case_alliance_stale_join_application_closed(summary);
    ok &= case_alliance_union_vote_absorption(summary);
    ok &= case_war_result_relation_scores(summary);
    ok &= case_war_settlement_alliance_exit(summary);
    ok &= case_war_settlement_edge_cases(summary);
    return ok;
}
