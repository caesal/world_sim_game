#include "game/game.h"

#include "core/game_state.h"
#include "render/panel_alliance_council.h"
#include "render/panel_alliance_model.h"
#include "render/panel_alliance_vote_state.h"
#include "render/render_common.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/regions.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define PROBE_DIR "build/validation/military_alliance_probe_20260625"

static int write_bmp_from_bits(const char *path, BITMAPINFO *info, void *bits, int width, int height) {
    FILE *f = fopen(path, "wb");
    int row_bytes = ((width * 32 + 31) / 32) * 4;
    int image_bytes = row_bytes * height;
    unsigned char header[54] = {'B', 'M'};
    if (!f) return 0;
    *(int *)&header[2] = 54 + image_bytes;
    *(int *)&header[10] = 54;
    *(int *)&header[14] = 40;
    *(int *)&header[18] = width;
    *(int *)&header[22] = height;
    *(short *)&header[26] = 1;
    *(short *)&header[28] = 32;
    *(int *)&header[34] = image_bytes;
    fwrite(header, 1, sizeof(header), f);
    fwrite(bits, 1, image_bytes, f);
    fclose(f);
    (void)info;
    return 1;
}

static void setup_council_snapshot(RenderSnapshot *snap, AlliancePanelRow *row) {
    AllianceSnapshotRecord *record;
    int i, units[3] = {414, 343, 243}, pop[3] = {400, 400, 200}, provinces[3] = {429, 286, 286};
    memset(snap, 0, sizeof(*snap));
    memset(row, 0, sizeof(*row));
    snap->year = 1205;
    snap->civ_count = 3;
    snap->alliance_count = 1;
    for (i = 0; i < 3; i++) {
        snap->civs[i].alive = 1;
        snap->civs[i].color = i == 0 ? RGB(68, 150, 186) : (i == 1 ? RGB(96, 172, 102) : RGB(188, 78, 136));
        snap->civs[i].symbol = (char)('A' + i);
        snprintf(snap->civs[i].name_en, sizeof(snap->civs[i].name_en), "Country %c", 'A' + i);
        snprintf(snap->civs[i].name_zh, sizeof(snap->civs[i].name_zh), "Country %c", 'A' + i);
    }
    record = &snap->alliances[0];
    record->active = 1;
    record->id = 0;
    record->type = ALLIANCE_TYPE_MILITARY;
    record->member_count = 3;
    record->council_last_election_year = 1200;
    record->council_next_election_year = 1216;
    snprintf(record->name_en, sizeof(record->name_en), "Probe Alliance");
    snprintf(record->name_zh, sizeof(record->name_zh), "Probe Alliance");
    for (i = 0; i < 3; i++) {
        record->members[i] = i;
        record->council_vote_units[i] = units[i];
        record->council_population_permille[i] = pop[i];
        record->council_province_permille[i] = provinces[i];
    }
    row->kind = ALLIANCE_PANEL_ROW_ALLIANCE;
    row->alliance_id = 0;
    row->type = ALLIANCE_TYPE_MILITARY;
    row->member_count = 3;
}

static void setup_even_council_snapshot(RenderSnapshot *snap, AlliancePanelRow *row) {
    AllianceSnapshotRecord *record;
    int i;
    memset(snap, 0, sizeof(*snap));
    memset(row, 0, sizeof(*row));
    snap->year = 1205;
    snap->civ_count = 2;
    snap->alliance_count = 1;
    for (i = 0; i < 2; i++) {
        snap->civs[i].alive = 1;
        snap->civs[i].color = i == 0 ? RGB(68, 150, 186) : RGB(188, 78, 136);
        snap->civs[i].symbol = (char)('A' + i);
        snprintf(snap->civs[i].name_en, sizeof(snap->civs[i].name_en), "Country %c", 'A' + i);
        snprintf(snap->civs[i].name_zh, sizeof(snap->civs[i].name_zh), "Country %c", 'A' + i);
    }
    record = &snap->alliances[0];
    record->active = 1;
    record->id = 0;
    record->type = ALLIANCE_TYPE_MILITARY;
    record->member_count = 2;
    record->council_last_election_year = 1200;
    record->council_next_election_year = 1216;
    snprintf(record->name_en, sizeof(record->name_en), "Even Alliance");
    snprintf(record->name_zh, sizeof(record->name_zh), "Even Alliance");
    for (i = 0; i < 2; i++) {
        record->members[i] = i;
        record->council_vote_units[i] = 500;
        record->council_population_permille[i] = 500;
        record->council_province_permille[i] = 500;
    }
    row->kind = ALLIANCE_PANEL_ROW_ALLIANCE;
    row->alliance_id = 0;
    row->type = ALLIANCE_TYPE_MILITARY;
    row->member_count = 2;
}

static int render_council_bmp(FILE *out, const char *file_name, int even_split) {
    enum { W = 430, H = 590 };
    static RenderSnapshot snap;
    AlliancePanelRow row;
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap = NULL, old_bitmap = NULL;
    void *bits = NULL;
    UiCursor cursor;
    char path[256];
    int ok = 0, old_lang = ui_language;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = W;
    info.bmiHeader.biHeight = H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !dc) goto cleanup;
    old_bitmap = (HBITMAP)SelectObject(dc, bitmap);
    memset(bits, 22, W * H * 4);
    if (even_split) setup_even_council_snapshot(&snap, &row);
    else setup_council_snapshot(&snap, &row);
    cursor = ui_cursor(14, 14, W - 28, H - 14);
    ui_language = UI_LANG_ZH;
    alliance_council_draw_overview(dc, &cursor, &snap, &row);
    snprintf(path, sizeof(path), "%s/%s", PROBE_DIR, file_name);
    ok = write_bmp_from_bits(path, &info, bits, W, H);
cleanup:
    ui_language = old_lang;
    if (old_bitmap) SelectObject(dc, old_bitmap);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    if (screen) ReleaseDC(NULL, screen);
    fprintf(out, "case=%s ok=%d artifact=%s\n",
            even_split ? "council_even_split_artifact" : "council_render_artifact", ok, path);
    return ok;
}

static int case_display_votes(FILE *out) {
    static RenderSnapshot snap;
    AlliancePanelRow row;
    AllianceCouncilDisplayMember members[MAX_CIVS];
    int i, count, sum = 0, order;
    setup_council_snapshot(&snap, &row);
    snap.alliances[0].council_previous_valid = 1;
    snap.alliances[0].council_previous_vote_units[0] = 500;
    snap.alliances[0].council_previous_vote_units[1] = 300;
    snap.alliances[0].council_previous_vote_units[2] = 200;
    count = alliance_council_build_display_members(&snap.alliances[0], members, MAX_CIVS);
    for (i = 0; i < count; i++) sum += members[i].display_seats;
    order = count == 3 && members[0].display_seats > members[1].display_seats &&
            members[1].display_seats > members[2].display_seats;
    fprintf(out, "case=council_display_votes ok=%d seats=%d,%d,%d prev=%d,%d,%d sum=%d threshold_2_3=%d threshold_3_4=%d election=%d\n",
            count == 3 && sum == 80 && order &&
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 0) == 40 &&
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 1) == 24 &&
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 2) == 16 &&
            alliance_council_display_threshold_two_thirds() == 54 &&
            alliance_council_display_threshold_three_quarters() == 61 &&
            ALLIANCE_COUNCIL_ELECTION_YEARS == 16,
            members[0].display_seats, members[1].display_seats, members[2].display_seats,
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 0),
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 1),
            alliance_council_previous_display_seats_for_member(&snap.alliances[0], 2),
            sum, alliance_council_display_threshold_two_thirds(),
            alliance_council_display_threshold_three_quarters(), ALLIANCE_COUNCIL_ELECTION_YEARS);
    return count == 3 && sum == 80 && order &&
           alliance_council_previous_display_seats_for_member(&snap.alliances[0], 0) == 40 &&
           alliance_council_previous_display_seats_for_member(&snap.alliances[0], 1) == 24 &&
           alliance_council_previous_display_seats_for_member(&snap.alliances[0], 2) == 16 &&
           alliance_council_display_threshold_two_thirds() == 54 &&
           alliance_council_display_threshold_three_quarters() == 61 &&
           ALLIANCE_COUNCIL_ELECTION_YEARS == 16;
}

static int case_even_split_visual(FILE *out) {
    static RenderSnapshot snap;
    AlliancePanelRow row;
    AllianceCouncilDisplayMember members[MAX_CIVS];
    AllianceCouncilSeatVisual seats[ALLIANCE_COUNCIL_DISPLAY_SEATS];
    RECT slots[ALLIANCE_COUNCIL_DISPLAY_SEATS];
    RECT chart = {24, 58, 396, 358};
    int count, slot_count, seat_count;
    setup_even_council_snapshot(&snap, &row);
    count = alliance_council_build_display_members(&snap.alliances[0], members, MAX_CIVS);
    slot_count = alliance_council_build_chamber_slots(chart, slots, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    seat_count = alliance_council_build_visual_seats(&snap.alliances[0], chart, seats, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    fprintf(out, "case=council_even_split ok=%d seats=%d/%d slots=%d visual=%d first=%d mid=%d last=%d\n",
            count == 2 && members[0].display_seats == 40 && members[1].display_seats == 40 &&
            slot_count == 80 && seat_count == 80 && seats[0].member_civ == members[0].member_civ &&
            seats[39].member_civ == members[0].member_civ && seats[40].member_civ == members[1].member_civ &&
            seats[79].member_civ == members[1].member_civ,
            members[0].display_seats, members[1].display_seats, slot_count, seat_count,
            seats[0].member_civ, seats[40].member_civ, seats[79].member_civ);
    return count == 2 && members[0].display_seats == 40 && members[1].display_seats == 40 &&
           slot_count == 80 && seat_count == 80 && seats[0].member_civ == members[0].member_civ &&
           seats[39].member_civ == members[0].member_civ && seats[40].member_civ == members[1].member_civ &&
           seats[79].member_civ == members[1].member_civ &&
           render_council_bmp(out, "council_even_split_render.bmp", 1);
}

static int case_vote_year_snapshot(FILE *out) {
    static RenderSnapshot snap;
    AllianceSnapshotRecord *record;
    AllianceVoteRecord *vote, *fail_vote;
    AllianceCandidateRecord candidate;
    int i, yes, no, rows, candidate_rows, candidate_seats, fail_yes;
    memset(&snap, 0, sizeof(snap));
    snap.civ_count = 4; snap.alliance_count = 1; record = &snap.alliances[0];
    record->active = 1; record->id = 0; record->type = ALLIANCE_TYPE_MILITARY;
    record->member_count = 3; record->vote_count = 2; record->vote_next = 2;
    for (i = 0; i < 4; i++) { record->members[i] = i; record->joined_year_by_civ[i] = i == 3 ? 101 : 10; }
    record->council_vote_units[0] = 300; record->council_vote_units[1] = 190;
    record->council_vote_units[2] = 160; record->council_vote_units[3] = 350;
    vote = &record->votes[0]; memset(vote, 0, sizeof(*vote));
    vote->active = 1; vote->vote_type = ALLIANCE_VOTE_JOIN; vote->target_civ_id = 3;
    vote->vote_year = 100; vote->passed = 1; vote->yes_count = 775; vote->no_count = 225;
    for (i = 0; i < MAX_CIVS; i++) vote->member_votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    vote->member_votes[0] = vote->member_votes[1] = ALLIANCE_MEMBER_VOTE_YES;
    vote->member_votes[2] = ALLIANCE_MEMBER_VOTE_NO;
    record->vote_council_valid[0] = 1;
    record->vote_council_units[0][0] = 500; record->vote_council_units[0][1] = 275;
    record->vote_council_units[0][2] = 225;
    fail_vote = &record->votes[1]; memset(fail_vote, 0, sizeof(*fail_vote));
    fail_vote->active = 1; fail_vote->vote_type = ALLIANCE_VOTE_JOIN; fail_vote->target_civ_id = 3;
    fail_vote->vote_year = 102; fail_vote->passed = 0; fail_vote->yes_count = 750; fail_vote->no_count = 250;
    for (i = 0; i < MAX_CIVS; i++) fail_vote->member_votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    fail_vote->member_votes[0] = ALLIANCE_MEMBER_VOTE_YES; fail_vote->member_votes[1] = ALLIANCE_MEMBER_VOTE_NO;
    record->vote_council_valid[1] = 1; record->vote_council_units[1][0] = 750; record->vote_council_units[1][1] = 250;
    memset(&candidate, 0, sizeof(candidate)); candidate.active = 1; candidate.type = ALLIANCE_CANDIDATE_JOIN; candidate.civ_id = 3;
    yes = alliance_council_display_seats_for_vote(record, vote, ALLIANCE_MEMBER_VOTE_YES);
    no = alliance_council_display_seats_for_vote(record, vote, ALLIANCE_MEMBER_VOTE_NO);
    rows = alliance_vote_state_vote_member_count(record, vote);
    record->member_count = 3; record->members[0] = 0; record->members[1] = 1; record->members[2] = 3;
    candidate_rows = alliance_vote_state_candidate_member_count(&snap, record, &candidate, vote);
    candidate_seats = alliance_council_display_seats_for_vote_member(record, vote, 3);
    fail_yes = alliance_council_display_seats_for_vote(record, fail_vote, ALLIANCE_MEMBER_VOTE_YES);
    fprintf(out, "case=military_join_vote_snapshot ok=%d yes=%d no=%d rows=%d candidate_rows=%d candidate_seats=%d fail_yes=%d fail_passed=%d\n",
            yes >= 61 && yes + no == 80 && rows == 3 && candidate_rows == 2 &&
            candidate_seats == 0 && fail_yes == 60 && !fail_vote->passed,
            yes, no, rows, candidate_rows, candidate_seats, fail_yes, fail_vote->passed);
    return yes >= 61 && yes + no == 80 && rows == 3 && candidate_rows == 2 &&
           candidate_seats == 0 && fail_yes == 60 && !fail_vote->passed;
}

static int case_rule_constants(FILE *out) {
    int removal_ok = alliance_removal_vote_yes_chance_for_relation(14) == 100 &&
                     alliance_removal_vote_yes_chance_for_relation(15) == 100 &&
                     alliance_removal_vote_yes_chance_for_relation(59) == 35 &&
                     alliance_removal_vote_yes_chance_for_relation(60) == 0;
    int join_ok = alliance_military_join_vote_yes_chance_for_relation(79) == 0 &&
                  alliance_military_join_vote_yes_chance_for_relation(80) == 15 &&
                  alliance_military_join_vote_yes_chance_for_relation(100) == 50;
    int timing_ok = ALLIANCE_JOIN_FIRST_VOTE_YEARS == 30 && ALLIANCE_JOIN_RETRY_VOTE_YEARS == 10 &&
                    ALLIANCE_REMOVAL_FIRST_VOTE_YEARS == 30 && ALLIANCE_REMOVAL_RETRY_VOTE_YEARS == 10;
    int union_ok = alliance_union_required_years_for_type(ALLIANCE_TYPE_DEFENSIVE) == 800 &&
                   alliance_union_required_years_for_type(ALLIANCE_TYPE_MILITARY) == 500;
    int union_chance_ok = alliance_union_vote_yes_chance_from_ratio_permille(950) == 15 &&
                          alliance_union_vote_yes_chance_from_ratio_permille(700) == 35 &&
                          alliance_union_vote_yes_chance_from_ratio_permille(500) == 50 &&
                          alliance_union_vote_yes_chance_from_ratio_permille(250) == 70;
    fprintf(out, "case=rule_constants ok=%d removal=14:%d,15:%d,59:%d,60:%d join=79:%d,80:%d,100:%d timing=30/10 union=800/500 union_chance=950:%d,700:%d,500:%d,250:%d upgrade_yes=%d\n",
            removal_ok && join_ok && timing_ok && union_ok && union_chance_ok &&
            ALLIANCE_MILITARY_UPGRADE_YES_CHANCE == 60,
            alliance_removal_vote_yes_chance_for_relation(14),
            alliance_removal_vote_yes_chance_for_relation(15),
            alliance_removal_vote_yes_chance_for_relation(59),
            alliance_removal_vote_yes_chance_for_relation(60),
            alliance_military_join_vote_yes_chance_for_relation(79),
            alliance_military_join_vote_yes_chance_for_relation(80),
            alliance_military_join_vote_yes_chance_for_relation(100),
            alliance_union_vote_yes_chance_from_ratio_permille(950),
            alliance_union_vote_yes_chance_from_ratio_permille(700),
            alliance_union_vote_yes_chance_from_ratio_permille(500),
            alliance_union_vote_yes_chance_from_ratio_permille(250),
            ALLIANCE_MILITARY_UPGRADE_YES_CHANCE);
    return removal_ok && join_ok && timing_ok && union_ok && union_chance_ok &&
           ALLIANCE_MILITARY_UPGRADE_YES_CHANCE == 60;
}

static int case_union_proposer_timing(FILE *out) {
    AllianceSaveState *state;
    int id, i, first_vote = 0, blocked_retry, cooldown;
    alliance_reset(); diplomacy_reset(); event_log_clear();
    civ_count = 3; region_count = 5; year = 0; month = 1;
    memset(civs, 0, sizeof(civs)); memset(natural_regions, 0, sizeof(natural_regions));
    for (i = 0; i < civ_count; i++) {
        civs[i].alive = 1; civs[i].population = i == 0 ? 100 : 10000;
        civs[i].governance = civs[i].cohesion = 5; civs[i].capital_city = -1;
    }
    for (i = 0; i < region_count; i++) {
        natural_regions[i].alive = 1;
        natural_regions[i].owner_civ = i == 0 ? 0 : (i < 3 ? 1 : 2);
    }
    id = alliance_debug_create_pair(0, 1, 0);
    if (id < 0) {
        fprintf(out, "case=union_proposer_timing ok=0 vote=0 cooldown=0 blocked_retry=0 active=0\n");
        return 0;
    }
    year = 5; alliance_debug_add_member(id, 2, 0);
    year = 805; alliance_union_try(id);
    state = alliance_internal_state();
    for (i = 0; i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        AllianceVoteRecord *v = &state->votes[id][i];
        if (v->active && v->vote_type == ALLIANCE_VOTE_UNION &&
            v->target_civ_id == 0 && !v->passed) first_vote = 1;
    }
    cooldown = alliance_union_proposer_cooldown_remaining(id, 0);
    year = 810; blocked_retry = !alliance_union_try(id);
    fprintf(out, "case=union_proposer_timing ok=%d vote=%d cooldown=%d blocked_retry=%d active=%d\n",
            id >= 0 && first_vote && cooldown == 25 && blocked_retry &&
            state->records[id].active, first_vote, cooldown, blocked_retry,
            state->records[id].active);
    return id >= 0 && first_vote && cooldown == 25 && blocked_retry &&
           state->records[id].active;
}

int run_military_alliance_rules_probe(FILE *out) {
    int ok = 1;
    ok &= case_display_votes(out);
    ok &= case_even_split_visual(out);
    ok &= case_vote_year_snapshot(out);
    ok &= case_rule_constants(out);
    ok &= case_union_proposer_timing(out);
    ok &= render_council_bmp(out, "council_reference_render.bmp", 0);
    return ok;
}
