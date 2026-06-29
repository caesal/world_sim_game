#include "game/game.h"

#include "core/game_state.h"
#include "io/map_save_state.h"
#include "render/panel_alliance_council.h"
#include "render/panel_alliance_detail.h"
#include "render/panel_alliance_model.h"
#include "sim/alliance.h"
#include "sim/alliance_military.h"
#include "sim/diplomacy.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/war.h"
#include "sim/war_internal.h"
#include "sim/war_resolution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PROBE_DIR "build/validation/military_alliance_probe_20260625"

int run_military_alliance_rules_probe(FILE *out);

static void ensure_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(PROBE_DIR, NULL);
}

static void set_relation_score(int a, int b, int score) {
    DiplomacyRelation rel;
    memset(&rel, 0, sizeof(rel));
    rel.state = DIPLOMACY_PEACE;
    rel.relation_score = score;
    rel.years_known = 50;
    rel.overlord = -1;
    rel.vassal = -1;
    rel.last_war_winner = -1;
    rel.last_war_loser = -1;
    diplomacy_restore_relation(a, b, rel);
}

static void set_pair_score(int a, int b, int score) {
    set_relation_score(a, b, score);
    set_relation_score(b, a, score);
}

static void init_civ(int id, const char *name, int pop, int provinces) {
    int i, start = id * 40;
    memset(&civs[id], 0, sizeof(civs[id]));
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].alive = 1;
    civs[id].custom_name = 1;
    civs[id].capital_city = id;
    civs[id].color = COLOR32_RGB(70 + id * 35, 100 + id * 21, 150 + id * 13);
    civs[id].cohesion = 8;
    civs[id].military = 8;
    civs[id].population = pop;
    civs[id].treasury = 1000;
    civs[id].treasury_cap = 5000;
    memset(&cities[id], 0, sizeof(cities[id]));
    cities[id].alive = 1;
    cities[id].owner = id;
    cities[id].x = 2 + id * 2;
    cities[id].y = 2;
    cities[id].population = pop;
    cities[id].capital = 1;
    population_init_city(id, pop);
    for (i = 0; i < provinces; i++) {
        int r = start + i;
        natural_regions[r].alive = 1;
        natural_regions[r].owner_civ = id;
        natural_regions[r].city_id = id;
    }
}

static void reset_fixture(void) {
    int i;
    alliance_reset();
    diplomacy_reset();
    war_reset();
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities));
    memset(natural_regions, 0, sizeof(natural_regions));
    civ_count = 5;
    city_count = 5;
    region_count = 180;
    world_generated = 1;
    year = 1000;
    month = 1;
    init_civ(0, "A", 1000000, 30);
    init_civ(1, "B", 1000000, 20);
    init_civ(2, "C", 500000, 20);
    init_civ(3, "D", 800000, 10);
    init_civ(4, "E", 800000, 10);
    for (i = 0; i < civ_count; i++) {
        int j;
        for (j = 0; j < civ_count; j++) if (i != j) set_relation_score(i, j, 96);
    }
    population_sync_all();
}

static void bmp_u16(FILE *f, int v) {
    fputc(v & 255, f);
    fputc((v >> 8) & 255, f);
}

static void bmp_u32(FILE *f, int v) {
    fputc(v & 255, f);
    fputc((v >> 8) & 255, f);
    fputc((v >> 16) & 255, f);
    fputc((v >> 24) & 255, f);
}

static void bmp_pixel(unsigned char *pixels, int w, int h, int x, int y, COLORREF color) {
    unsigned char *p;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    p = pixels + (y * w + x) * 3;
    p[0] = (unsigned char)GetBValue(color);
    p[1] = (unsigned char)GetGValue(color);
    p[2] = (unsigned char)GetRValue(color);
}

static void bmp_rect(unsigned char *pixels, int w, int h, RECT r, COLORREF color) {
    int x, y;
    for (y = r.top; y < r.bottom; y++)
        for (x = r.left; x < r.right; x++) bmp_pixel(pixels, w, h, x, y, color);
}

static void bmp_draw_seat(unsigned char *pixels, int w, int h, RECT r, COLORREF color) {
    int x, y, cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int radius = max(2, min(r.right - r.left, r.bottom - r.top) / 2);
    for (y = r.top; y <= r.bottom; y++) {
        for (x = r.left; x <= r.right; x++) {
            int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            if (d2 <= radius * radius) {
                bmp_pixel(pixels, w, h, x, y,
                          d2 >= (radius - 1) * (radius - 1) ? RGB(24, 22, 28) : color);
            }
        }
    }
}

static int write_council_bmp(const char *path, const RECT *slots, int slot_count,
                             const AllianceCouncilSeatVisual *seats, int seat_count) {
    enum { W = 420, H = 480 };
    static unsigned char pixels[W * H * 3];
    FILE *f;
    int i, y, pad = (4 - (W * 3) % 4) % 4, row_bytes = W * 3 + pad;
    memset(pixels, 24, sizeof(pixels));
    bmp_rect(pixels, W, H, (RECT){12, 10, W - 12, H - 10}, RGB(31, 48, 52));
    bmp_rect(pixels, W, H, (RECT){24, 22, 122, 48}, RGB(26, 38, 42));
    bmp_rect(pixels, W, H, (RECT){132, 22, 254, 48}, RGB(26, 38, 42));
    bmp_rect(pixels, W, H, (RECT){264, 22, W - 24, 48}, RGB(26, 38, 42));
    bmp_rect(pixels, W, H, (RECT){24, 58, W - 24, 358}, RGB(18, 34, 38));
    for (i = 0; i < slot_count; i++) bmp_draw_seat(pixels, W, H, slots[i], RGB(26, 48, 51));
    for (i = 0; i < seat_count; i++) {
        COLORREF color = i < 34 ? RGB(112, 142, 190) : (i < 62 ? RGB(176, 108, 94) : RGB(116, 160, 104));
        bmp_draw_seat(pixels, W, H, seats[i].rect, color);
    }
    bmp_rect(pixels, W, H, (RECT){194, 332, 226, 336}, RGB(218, 172, 82));
    bmp_rect(pixels, W, H, (RECT){200, 322, 220, 326}, RGB(218, 172, 82));
    bmp_rect(pixels, W, H, (RECT){24, 368, W - 24, 388}, RGB(23, 42, 47));
    bmp_rect(pixels, W, H, (RECT){24, 391, W - 24, 413}, RGB(18, 32, 36));
    bmp_rect(pixels, W, H, (RECT){24, 415, W - 24, 437}, RGB(20, 36, 40));
    f = fopen(path, "wb");
    if (!f) return 0;
    fputc('B', f); fputc('M', f);
    bmp_u32(f, 54 + row_bytes * H);
    bmp_u16(f, 0); bmp_u16(f, 0); bmp_u32(f, 54);
    bmp_u32(f, 40); bmp_u32(f, W); bmp_u32(f, H);
    bmp_u16(f, 1); bmp_u16(f, 24); bmp_u32(f, 0);
    bmp_u32(f, row_bytes * H); bmp_u32(f, 2835); bmp_u32(f, 2835);
    bmp_u32(f, 0); bmp_u32(f, 0);
    for (y = H - 1; y >= 0; y--) {
        fwrite(pixels + y * W * 3, 1, W * 3, f);
        for (i = 0; i < pad; i++) fputc(0, f);
    }
    fclose(f);
    return 1;
}

static int make_three_member_alliance(void) {
    int id = alliance_debug_create_pair(0, 1, 95);
    if (id >= 0) alliance_debug_add_member(id, 2, 95);
    return id;
}

static int case_council_math(FILE *out) {
    AllianceSaveState *state;
    AllianceSnapshotRecord snap;
    int id, sum, order, visual_sum, fallback_ok, u0, u1, u2;
    reset_fixture();
    id = make_three_member_alliance();
    state = alliance_internal_state();
    alliance_council_recalculate(id, 0);
    sum = state->council_vote_units[id][0] + state->council_vote_units[id][1] +
          state->council_vote_units[id][2];
    u0 = state->council_vote_units[id][0];
    u1 = state->council_vote_units[id][1];
    u2 = state->council_vote_units[id][2];
    order = state->council_vote_units[id][0] > state->council_vote_units[id][1] &&
            state->council_vote_units[id][1] > state->council_vote_units[id][2];
    memset(&snap, 0, sizeof(snap));
    snap.member_count = 3;
    snap.members[0] = 0; snap.members[1] = 1; snap.members[2] = 2;
    memcpy(snap.council_vote_units, state->council_vote_units[id], sizeof(snap.council_vote_units));
    visual_sum = alliance_council_display_seats_for_member(&snap, 0) +
                 alliance_council_display_seats_for_member(&snap, 1) +
                 alliance_council_display_seats_for_member(&snap, 2);
    civs[0].population = civs[1].population = civs[2].population = 0;
    region_count = 0;
    alliance_council_recalculate(id, 0);
    fallback_ok = state->council_vote_units[id][0] + state->council_vote_units[id][1] +
                  state->council_vote_units[id][2] == ALLIANCE_COUNCIL_TOTAL_UNITS;
    fprintf(out, "case=council_math ok=%d units=%d,%d,%d sum=%d order=%d visual=%d fallback=%d\n",
            sum == 1000 && order && visual_sum == 80 && fallback_ok,
            u0, u1, u2, sum, order, visual_sum, fallback_ok);
    return sum == 1000 && order && visual_sum == 80 && fallback_ok;
}

static int case_council_visual_geometry(FILE *out) {
    AllianceSaveState *state;
    AllianceSnapshotRecord snap;
    AllianceCouncilSeatVisual seats[ALLIANCE_COUNCIL_VISUAL_SEATS];
    RECT slots[120];
    int id, i, j, seat_count, slot_count, distinct_y = 0, contiguous = 1, first_member, last_member;
    int min_x = 9999, min_y = 9999, max_x = -9999, max_y = -9999, max_d = 0, min_d2 = 999999;
    int center_x, chart_w, left_arm_bottom = -9999, right_arm_bottom = -9999, top_center = 9999;
    int y_seen[260], member_seen[MAX_CIVS];
    RECT chart = {24, 58, 396, 358};
    reset_fixture();
    id = make_three_member_alliance();
    state = alliance_internal_state();
    alliance_council_recalculate(id, 0);
    memset(&snap, 0, sizeof(snap));
    snap.active = 1;
    snap.id = id;
    snap.member_count = 3;
    snap.members[0] = 0; snap.members[1] = 1; snap.members[2] = 2;
    memcpy(snap.council_vote_units, state->council_vote_units[id], sizeof(snap.council_vote_units));
    slot_count = alliance_council_build_chamber_slots(chart, slots, 120);
    seat_count = alliance_council_build_visual_seats(&snap, chart, seats, ALLIANCE_COUNCIL_VISUAL_SEATS);
    center_x = (chart.left + chart.right) / 2;
    chart_w = chart.right - chart.left;
    memset(y_seen, 0, sizeof(y_seen));
    memset(member_seen, 0, sizeof(member_seen));
    first_member = seat_count > 0 ? seats[0].member_civ : -1;
    last_member = first_member;
    for (i = 0; i < seat_count; i++) {
        int y = clamp(seats[i].rect.top, 0, (int)(sizeof(y_seen) / sizeof(y_seen[0])) - 1);
        if (!y_seen[y]) { y_seen[y] = 1; distinct_y++; }
        if (member_seen[seats[i].member_civ] && seats[i].member_civ != last_member) contiguous = 0;
        member_seen[seats[i].member_civ] = 1;
        last_member = seats[i].member_civ;
        min_x = min(min_x, seats[i].rect.left);
        min_y = min(min_y, seats[i].rect.top);
        max_x = max(max_x, seats[i].rect.right);
        max_y = max(max_y, seats[i].rect.bottom);
        max_d = max(max_d, seats[i].rect.right - seats[i].rect.left);
        {
            int ax = (seats[i].rect.left + seats[i].rect.right) / 2;
            int ay = (seats[i].rect.top + seats[i].rect.bottom) / 2;
            if (ax < chart.left + chart_w / 4) left_arm_bottom = max(left_arm_bottom, ay);
            if (ax > chart.right - chart_w / 4) right_arm_bottom = max(right_arm_bottom, ay);
            if (abs(ax - center_x) < chart_w / 8) top_center = min(top_center, ay);
        }
        for (j = 0; j < i; j++) {
            int ax = (seats[i].rect.left + seats[i].rect.right) / 2;
            int ay = (seats[i].rect.top + seats[i].rect.bottom) / 2;
            int bx = (seats[j].rect.left + seats[j].rect.right) / 2;
            int by = (seats[j].rect.top + seats[j].rect.bottom) / 2;
            int dx = ax - bx, dy = ay - by;
            min_d2 = min(min_d2, dx * dx + dy * dy);
        }
    }
    write_council_bmp(PROBE_DIR "/council_hemicycle.bmp", slots, slot_count, seats, seat_count);
    fprintf(out, "case=council_visual_geometry ok=%d seats=%d slots=%d dark=%d distinct_y=%d bbox=%dx%d seat_d=%d min_d2=%d side_drop=%d/%d contiguous=%d first=%d last=%d artifact=%s\n",
            seat_count == ALLIANCE_COUNCIL_DISPLAY_SEATS &&
            slot_count == ALLIANCE_COUNCIL_DISPLAY_SEATS &&
            slot_count - seat_count == 0 && distinct_y >= 34 && max_y - min_y >= 145 &&
            max_y - min_y <= 220 && max_x - min_x > 350 && max_d >= 19 && min_d2 >= 360 &&
            left_arm_bottom - top_center >= 95 && left_arm_bottom - top_center <= 160 &&
            right_arm_bottom - top_center >= 95 && right_arm_bottom - top_center <= 160 && contiguous &&
            first_member == 0 && member_seen[0] && member_seen[1] && member_seen[2],
            seat_count, slot_count, slot_count - seat_count, distinct_y, max_x - min_x, max_y - min_y,
            max_d, min_d2, left_arm_bottom - top_center, right_arm_bottom - top_center,
            contiguous, first_member, last_member,
            PROBE_DIR "/council_hemicycle.bmp");
    return seat_count == ALLIANCE_COUNCIL_DISPLAY_SEATS &&
           slot_count == ALLIANCE_COUNCIL_DISPLAY_SEATS &&
           slot_count - seat_count == 0 && distinct_y >= 34 && max_y - min_y >= 145 &&
           max_y - min_y <= 220 && max_x - min_x > 350 && max_d >= 19 && min_d2 >= 360 &&
           left_arm_bottom - top_center >= 95 && left_arm_bottom - top_center <= 160 &&
           right_arm_bottom - top_center >= 95 && right_arm_bottom - top_center <= 160 && contiguous &&
           first_member == 0 && member_seen[0] && member_seen[1] && member_seen[2];
}

static int case_alliance_list_grouping(FILE *out) {
    static RenderSnapshot snap;
    const AlliancePanelModel *model;
    int i, ok;
    memset(&snap, 0, sizeof(snap));
    snap.revision = 1001;
    snap.alliance_revision = 1002;
    snap.civs_revision = 1003;
    snap.diplomacy_revision = 1004;
    snap.year = 1200;
    snap.civ_count = 3;
    snap.alliance_count = 2;
    for (i = 0; i < 3; i++) {
        snap.civs[i].alive = 1;
        snap.civs[i].overlord = -1;
        snap.civs[i].alliance_display_id = -1;
        snap.civs[i].color = RGB(80 + i * 40, 110 + i * 30, 150 + i * 20);
        snap.civs[i].current_soldiers = 100 + i * 100;
        snap.civs[i].treasury = 100 + i * 100;
        snap.civs[i].summary.cities = 1 + i;
        snap.civs[i].tech_stage = i;
        snprintf(snap.civs[i].name_en, sizeof(snap.civs[i].name_en), "Probe Civ %d", i);
        snprintf(snap.civs[i].name_zh, sizeof(snap.civs[i].name_zh), "Probe Civ %d", i);
    }
    snap.civs[0].alliance_display_id = 0;
    snap.civs[1].alliance_display_id = 1;
    snap.civs[0].summary.population = 1000;
    snap.civs[1].summary.population = 100;
    snap.civs[2].summary.population = 9000000;
    snap.alliances[0].active = 1;
    snap.alliances[0].id = 0;
    snap.alliances[0].type = ALLIANCE_TYPE_DEFENSIVE;
    snap.alliances[0].founder_civ_id = 0;
    snap.alliances[0].member_count = 1;
    snap.alliances[0].members[0] = 0;
    snap.alliances[1].active = 1;
    snap.alliances[1].id = 1;
    snap.alliances[1].type = ALLIANCE_TYPE_MILITARY;
    snap.alliances[1].founder_civ_id = 1;
    snap.alliances[1].member_count = 1;
    snap.alliances[1].members[0] = 1;
    model = alliance_panel_model_get(&snap, 0, 0, 1);
    ok = model && model->row_count >= 3 &&
         alliance_panel_row_type_group(&model->rows[0]) == 0 &&
         alliance_panel_row_type_group(&model->rows[1]) == 1 &&
         alliance_panel_row_type_group(&model->rows[2]) == 2 &&
         model->rows[0].alliance_id == 1 && model->rows[1].alliance_id == 0 &&
         model->rows[2].kind == ALLIANCE_PANEL_ROW_COUNTRY;
    fprintf(out, "case=alliance_list_grouping ok=%d groups=%d,%d,%d ids=%d,%d civ=%d\n",
            ok,
            model && model->row_count > 0 ? alliance_panel_row_type_group(&model->rows[0]) : -1,
            model && model->row_count > 1 ? alliance_panel_row_type_group(&model->rows[1]) : -1,
            model && model->row_count > 2 ? alliance_panel_row_type_group(&model->rows[2]) : -1,
            model && model->row_count > 0 ? model->rows[0].alliance_id : -99,
            model && model->row_count > 1 ? model->rows[1].alliance_id : -99,
            model && model->row_count > 2 ? model->rows[2].civ_id : -99);
    return ok;
}

static int case_election_and_upgrade(FILE *out) {
    AllianceSaveState *state;
    AllianceYearWork work;
    int id, before, at, age299, age300, directed_fail, initiated, passed, no_dup, i, upgrade_active = 0;
    reset_fixture();
    id = make_three_member_alliance();
    state = alliance_internal_state();
    before = state->council_next_election_year[id];
    year = before - 1; alliance_year_work_begin(&work);
    while (alliance_council_update_year_step(&work)) {}
    before = state->council_last_election_year[id] == 1000;
    year = 1000 + ALLIANCE_COUNCIL_ELECTION_YEARS; alliance_year_work_begin(&work);
    while (alliance_council_update_year_step(&work)) {}
    at = state->council_last_election_year[id] == 1000 + ALLIANCE_COUNCIL_ELECTION_YEARS;
    state->records[id].founded_year = year - 299;
    age299 = !alliance_military_eligible(id);
    state->records[id].founded_year = year - 300;
    set_relation_score(1, 2, 94);
    directed_fail = !alliance_military_eligible(id);
    set_relation_score(1, 2, 96);
    age300 = alliance_military_eligible(id);
    alliance_year_work_begin(&work);
    while (alliance_military_update_year_step(&work)) {}
    initiated = state->military_upgrade_active[id] && state->military_upgrade_start_year[id] == year;
    for (i = 0; i < ALLIANCE_CANDIDATE_RECORD_CAP; i++)
        if (state->candidates[id][i].active &&
            state->candidates[id][i].type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE &&
            state->candidates[id][i].status == ALLIANCE_CANDIDATE_ACTIVE) upgrade_active++;
    no_dup = upgrade_active == 1;
    srand(2);
    year++;
    alliance_year_work_begin(&work);
    while (alliance_military_update_year_step(&work)) {}
    passed = alliance_type(id) == ALLIANCE_TYPE_MILITARY && state->vote_count[id] > 0;
    fprintf(out, "case=election_upgrade ok=%d election_before=%d election_at=%d age299=%d age300=%d directed_fail=%d initiated=%d no_dup=%d passed=%d type=%d\n",
            before && at && age299 && age300 && directed_fail && initiated && no_dup && passed,
            before, at, age299, age300, directed_fail, initiated, no_dup, passed, alliance_type(id));
    return before && at && age299 && age300 && directed_fail && initiated && no_dup && passed;
}

static int case_upgrade_retry_and_save(FILE *out) {
    static AllianceSaveState saved;
    AllianceSaveState *state;
    AllianceYearWork work;
    int id, failed, blocked_early, saved_ok;
    reset_fixture();
    id = make_three_member_alliance();
    state = alliance_internal_state();
    year = 1400;
    state->records[id].founded_year = 1000;
    alliance_year_work_begin(&work);
    while (alliance_military_update_year_step(&work)) {}
    set_pair_score(1, 2, 80);
    year++;
    alliance_year_work_begin(&work);
    while (alliance_military_update_year_step(&work)) {}
    failed = alliance_type(id) == ALLIANCE_TYPE_DEFENSIVE &&
             state->military_upgrade_cooldown[id] == ALLIANCE_MILITARY_RETRY_YEARS;
    set_pair_score(1, 2, 96);
    year += ALLIANCE_MILITARY_RETRY_YEARS - 1;
    alliance_year_work_begin(&work);
    while (alliance_military_update_year_step(&work)) {}
    blocked_early = !state->military_upgrade_active[id];
    alliance_copy_save_state(&saved);
    alliance_reset();
    alliance_restore_save_state(&saved);
    state = alliance_internal_state();
    saved_ok = state->records[id].active && state->alliance_type[id] == ALLIANCE_TYPE_DEFENSIVE &&
               state->council_vote_units[id][0] > 0;
    fprintf(out, "case=upgrade_retry_save ok=%d failed=%d blocked_early=%d saved_ok=%d cooldown=%d\n",
            failed && blocked_early && saved_ok, failed, blocked_early, saved_ok,
            state->military_upgrade_cooldown[id]);
    return failed && blocked_early && saved_ok;
}

static int case_war_rules(FILE *out) {
    ActiveWar war, war2;
    AllianceSaveState *state;
    int id, support_one, support_split, casualties_before, casualties_after;
    int military_no_exit, defensive_exit;
    reset_fixture();
    id = make_three_member_alliance();
    state = alliance_internal_state();
    state->alliance_type[id] = ALLIANCE_TYPE_MILITARY;
    memset(&war, 0, sizeof(war));
    war.active = 1; war.attacker = 0; war.defender = 4;
    active_wars[0] = war;
    support_one = alliance_military_support_for_war(&war, 1);
    memset(&war2, 0, sizeof(war2));
    war2.active = 1; war2.attacker = 1; war2.defender = 4;
    active_wars[1] = war2;
    support_split = alliance_military_support_for_war(&war, 1);
    casualties_before = support_casualties[2];
    alliance_military_apply_support_casualties(&war, 1);
    casualties_after = support_casualties[2];
    reset_fixture();
    id = alliance_debug_create_pair(1, 2, 95);
    state = alliance_internal_state();
    state->alliance_type[id] = ALLIANCE_TYPE_MILITARY;
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 2, 1000, 100, DIP_LAST_WAR_MILITARY);
    military_no_exit = alliance_for_civ(1) == id;
    reset_fixture();
    id = alliance_debug_create_pair(1, 2, 95);
    war_apply_outcome_with_result(0, 1, WAR_OUTCOME_ATTACKER_WIN, 2, 1000, 100, DIP_LAST_WAR_MILITARY);
    defensive_exit = alliance_for_civ(1) < 0;
    fprintf(out, "case=war_rules ok=%d support_one=%d support_split=%d casualties=%d/%d military_no_exit=%d defensive_exit=%d\n",
            support_one > 0 && support_split > 0 && support_split < support_one &&
            casualties_after > casualties_before && military_no_exit && defensive_exit,
            support_one, support_split, casualties_before, casualties_after,
            military_no_exit, defensive_exit);
    return support_one > 0 && support_split > 0 && support_split < support_one &&
           casualties_after > casualties_before && military_no_exit && defensive_exit;
}

int run_military_alliance_probe(void) {
    FILE *out;
    int ok = 1;
    ensure_dirs();
    out = fopen(PROBE_DIR "/summary.txt", "w");
    if (!out) return 2;
    ok &= case_council_math(out);
    ok &= case_council_visual_geometry(out);
    ok &= case_alliance_list_grouping(out);
    ok &= case_election_and_upgrade(out);
    ok &= case_upgrade_retry_and_save(out);
    ok &= case_war_rules(out);
    ok &= run_military_alliance_rules_probe(out);
    fprintf(out, "probe=military_alliance ok=%d\n", ok);
    fclose(out);
    out = fopen(PROBE_DIR "/summary.txt", "r");
    if (out) {
        char line[256];
        while (fgets(line, sizeof(line), out)) fputs(line, stdout);
        fclose(out);
    }
    return ok ? 0 : 1;
}
