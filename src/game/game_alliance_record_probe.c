#include "game/game_alliance_record_probe.h"

#include "core/game_state.h"
#include "core/game_notifications.h"
#include "game/game_worldgen.h"
#include "io/map_save_state.h"
#include "sim/alliance.h"
#include "sim/alliance_contact.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_borders.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"

#include <stdio.h>
#include <string.h>

static void init_record_probe_civ(int id, const char *name) {
    memset(&civs[id], 0, sizeof(civs[id]));
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].capital_city = id;
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
        ok &= map_save_read_dynamic_state(file, 15) > 0;
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

static int case_alliance_union_auto_merge(FILE *summary) {
    AllianceSaveState *state;
    DiplomacyRelation rel;
    int id, new_civ, ok = 1, i, union_history = 0;
    set_active_map_size(MAP_SIZE_SMALL);
    alliance_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    event_log_clear();
    game_clear_world_tiles();
    civ_count = 3;
    city_count = 3;
    region_count = 3;
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
    init_union_region_city(1, 1, 5);
    init_union_region_city(2, 2, 9);
    world_recalculate_territory();
    id = alliance_debug_create_pair(0, 1, 0);
    rel = diplomacy_relation(0, 2);
    rel.state = DIPLOMACY_PEACE;
    rel.relation_score = 33;
    diplomacy_restore_relation(0, 2, rel);
    diplomacy_restore_relation(2, 0, rel);
    rel.state = DIPLOMACY_WAR;
    rel.relation_score = -80;
    diplomacy_restore_relation(1, 2, rel);
    diplomacy_restore_relation(2, 1, rel);
    year = 1700;
    ok &= id >= 0;
    alliance_update_year();
    new_civ = civ_count - 1;
    state = alliance_internal_state();
    for (i = 0; i < state->history_count[id] && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        if (state->history[id][i].active &&
            state->history[id][i].event_type == ALLIANCE_HISTORY_UNION_FORMED) union_history = 1;
    }
    ok &= new_civ >= 3 && civs[new_civ].alive;
    ok &= !civs[0].alive && !civs[1].alive;
    ok &= cities[0].owner == new_civ && cities[1].owner == new_civ;
    ok &= natural_regions[0].owner_civ == new_civ && natural_regions[1].owner_civ == new_civ;
    ok &= civs[new_civ].capital_city == 0;
    ok &= civs[new_civ].treasury == 2100;
    ok &= diplomacy_status(new_civ, 2) == DIPLOMACY_PEACE;
    ok &= state->records[id].active == 0 && alliance_for_civ(new_civ) < 0;
    ok &= union_history && event_log_total_entries > 0 && game_notifications_count() > 0;
    fprintf(summary,
            "case=alliance_union_auto_merge ok=%d alliance=%d new_civ=%d new_alive=%d old_alive=%d/%d owners=%d/%d treasury=%d status_to_neighbor=%d history=%d notifications=%d\n",
            ok, id, new_civ, civs[new_civ].alive, civs[0].alive, civs[1].alive,
            cities[0].owner, cities[1].owner, civs[new_civ].treasury,
            diplomacy_status(new_civ, 2), union_history, game_notifications_count());
    return ok;
}

int run_alliance_record_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_alliance_record_roundtrip(summary);
    ok &= case_alliance_diplomatic_contact(summary);
    ok &= case_alliance_retryable_vote_records(summary);
    ok &= case_alliance_stale_join_application_closed(summary);
    ok &= case_alliance_union_auto_merge(summary);
    return ok;
}
