#include "render/panel_alliance_council.h"

#include "render/render_common.h"
#include "sim/alliance.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int index;
    long long remainder;
} SeatRemainder;

typedef struct {
    RECT rect;
    int angle_milli;
    int radius;
} CouncilChamberSlot;

static double council_sin(double x) {
    double x2 = x * x;
    return x * (1.0 - x2 / 6.0 + x2 * x2 / 120.0 - x2 * x2 * x2 / 5040.0);
}

static double council_cos(double x) {
    double x2 = x * x;
    return 1.0 - x2 / 2.0 + x2 * x2 / 24.0 - x2 * x2 * x2 / 720.0;
}

static const char *civ_name(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return "-";
    return ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh : snapshot->civs[civ_id].name_en;
}

static int display_pass_threshold(int numerator, int denominator) {
    return ALLIANCE_COUNCIL_DISPLAY_SEATS * numerator / denominator + 1;
}

int alliance_council_display_threshold_two_thirds(void) {
    return display_pass_threshold(2, 3);
}

int alliance_council_display_threshold_three_quarters(void) {
    return display_pass_threshold(3, 4);
}

static void sort_display_members(AllianceCouncilDisplayMember *members, int count) {
    int i;
    for (i = 1; i < count; i++) {
        AllianceCouncilDisplayMember m = members[i];
        int j = i - 1;
        while (j >= 0 && (members[j].internal_units < m.internal_units ||
               (members[j].internal_units == m.internal_units && members[j].member_civ > m.member_civ))) {
            members[j + 1] = members[j];
            j--;
        }
        members[j + 1] = m;
    }
}

static void sort_remainders(SeatRemainder *remainders, int count,
                            const AllianceCouncilDisplayMember *members) {
    int i;
    for (i = 1; i < count; i++) {
        SeatRemainder r = remainders[i];
        int j = i - 1;
        while (j >= 0 && (remainders[j].remainder < r.remainder ||
               (remainders[j].remainder == r.remainder &&
                members[remainders[j].index].member_civ > members[r.index].member_civ))) {
            remainders[j + 1] = remainders[j];
            j--;
        }
        remainders[j + 1] = r;
    }
}

static void allocate_display_seats(AllianceCouncilDisplayMember *members, int count) {
    SeatRemainder remainders[MAX_CIVS];
    int i, assigned = 0, pool = ALLIANCE_COUNCIL_DISPLAY_SEATS;
    long long total = 0;
    if (!members || count <= 0) return;
    for (i = 0; i < count; i++) total += max(0, members[i].internal_units);
    if (total <= 0) return;
    if (count <= ALLIANCE_COUNCIL_DISPLAY_SEATS) {
        for (i = 0; i < count; i++) {
            members[i].display_seats = 1;
            assigned++;
        }
        pool -= count;
    }
    for (i = 0; i < count; i++) {
        long long scaled = (long long)members[i].internal_units * pool;
        int extra = total > 0 ? (int)(scaled / total) : 0;
        members[i].display_seats += extra;
        assigned += extra;
        remainders[i].index = i;
        remainders[i].remainder = total > 0 ? scaled % total : 0;
    }
    sort_remainders(remainders, count, members);
    for (i = 0; assigned < ALLIANCE_COUNCIL_DISPLAY_SEATS && i < count; i++, assigned++) {
        members[remainders[i].index].display_seats++;
    }
    while (assigned > ALLIANCE_COUNCIL_DISPLAY_SEATS) {
        int changed = 0;
        for (i = count - 1; i >= 0 && assigned > ALLIANCE_COUNCIL_DISPLAY_SEATS; i--) {
            if (members[i].display_seats > (count <= ALLIANCE_COUNCIL_DISPLAY_SEATS ? 1 : 0)) {
                members[i].display_seats--;
                assigned--;
                changed = 1;
            }
        }
        if (!changed) break;
    }
}

static int build_display_members_from_units(const int *units, AllianceCouncilDisplayMember *members, int cap) {
    int civ, count = 0;
    if (!units || !members || cap <= 0) return 0;
    for (civ = 0; civ < MAX_CIVS && count < cap; civ++) {
        if (units[civ] <= 0) continue;
        members[count].member_civ = civ;
        members[count].internal_units = units[civ];
        members[count].display_seats = 0;
        members[count].population_permille = 0;
        members[count].province_permille = 0;
        count++;
    }
    sort_display_members(members, count);
    allocate_display_seats(members, count);
    return count;
}

int alliance_council_build_display_members(const AllianceSnapshotRecord *record,
                                           AllianceCouncilDisplayMember *members, int cap) {
    int i, count = 0;
    if (!record || !members || cap <= 0) return 0;
    for (i = 0; i < record->member_count && i < MAX_CIVS && count < cap; i++) {
        int civ = record->members[i];
        int units = civ >= 0 && civ < MAX_CIVS ? record->council_vote_units[civ] : 0;
        if (civ < 0 || units <= 0) continue;
        members[count].member_civ = civ;
        members[count].internal_units = units;
        members[count].display_seats = 0;
        members[count].population_permille = record->council_population_permille[civ];
        members[count].province_permille = record->council_province_permille[civ];
        count++;
    }
    sort_display_members(members, count);
    allocate_display_seats(members, count);
    return count;
}

int alliance_council_display_seats_for_member(const AllianceSnapshotRecord *record, int civ_id) {
    AllianceCouncilDisplayMember members[MAX_CIVS];
    int i, count = alliance_council_build_display_members(record, members, MAX_CIVS);
    for (i = 0; i < count; i++) if (members[i].member_civ == civ_id) return members[i].display_seats;
    return 0;
}

static int vote_snapshot_index(const AllianceSnapshotRecord *record, const AllianceVoteRecord *vote) {
    uintptr_t base, ptr, span, stride;
    if (!record || !vote) return -1;
    base = (uintptr_t)&record->votes[0];
    ptr = (uintptr_t)vote;
    stride = sizeof(record->votes[0]);
    span = stride * ALLIANCE_VOTE_RECORD_CAP;
    if (ptr < base || ptr >= base + span || ((ptr - base) % stride) != 0) return -1;
    return (int)((ptr - base) / stride);
}

int alliance_council_vote_has_snapshot(const AllianceSnapshotRecord *record,
                                       const AllianceVoteRecord *vote) {
    int idx = vote_snapshot_index(record, vote);
    return idx >= 0 && record->vote_council_valid[idx];
}

int alliance_council_vote_units_for_member(const AllianceSnapshotRecord *record,
                                           const AllianceVoteRecord *vote, int civ_id) {
    int idx = vote_snapshot_index(record, vote);
    if (idx < 0 || civ_id < 0 || civ_id >= MAX_CIVS || !record->vote_council_valid[idx]) return 0;
    return record->vote_council_units[idx][civ_id];
}

int alliance_council_display_seats_for_vote_member(const AllianceSnapshotRecord *record,
                                                   const AllianceVoteRecord *vote, int civ_id) {
    AllianceCouncilDisplayMember members[MAX_CIVS];
    int i, idx = vote_snapshot_index(record, vote);
    if (idx >= 0 && record->vote_council_valid[idx]) {
        int count = build_display_members_from_units(record->vote_council_units[idx], members, MAX_CIVS);
        for (i = 0; i < count; i++) if (members[i].member_civ == civ_id) return members[i].display_seats;
        return 0;
    }
    return alliance_council_display_seats_for_member(record, civ_id);
}

int alliance_council_display_seats_for_vote(const AllianceSnapshotRecord *record,
                                            const AllianceVoteRecord *vote, int vote_value) {
    AllianceCouncilDisplayMember members[MAX_CIVS];
    int i, total = 0, idx = vote_snapshot_index(record, vote);
    int count = idx >= 0 && record->vote_council_valid[idx] ?
                build_display_members_from_units(record->vote_council_units[idx], members, MAX_CIVS) :
                alliance_council_build_display_members(record, members, MAX_CIVS);
    for (i = 0; vote && i < count; i++) {
        int civ = members[i].member_civ;
        if (civ >= 0 && civ < MAX_CIVS && vote->member_votes[civ] == vote_value)
            total += members[i].display_seats;
    }
    return total;
}

static int build_chamber_slot_meta(RECT chart, CouncilChamberSlot *out, int out_cap) {
    static const int row_counts[5] = {8, 12, 16, 20, 24};
    static const int row_radius[5] = {360, 450, 540, 630, 720};
    static const int row_half_angle_milli[5] = {1050, 1120, 1190, 1260, 1330};
    int row, seat = 0;
    int width = chart.right - chart.left;
    int height = chart.bottom - chart.top;
    int seat_w = clamp((width + 12) / 20, 19, 20);
    int seat_h = seat_w;
    if (!out || out_cap <= 0) return 0;
    for (row = 0; row < 5 && seat < out_cap; row++) {
        int idx, count = row_counts[row];
        for (idx = 0; idx < count && seat < out_cap; idx++, seat++) {
            double t = ((double)idx + 0.5) / (double)count;
            double angle = (2.0 * t - 1.0) * row_half_angle_milli[row] / 1000.0;
            int x_norm = 500 + (int)(row_radius[row] * 680 * council_sin(angle) / 1000.0 +
                                      (angle >= 0.0 ? 0.5 : -0.5));
            int y_norm = 690 - (int)(row_radius[row] * 850 * council_cos(angle) / 1000.0 + 0.5);
            int x = chart.left + width * x_norm / 1000;
            int y = chart.top + height * y_norm / 1000;
            out[seat].rect = (RECT){x - seat_w / 2, y - seat_h / 2,
                                    x + seat_w / 2 + 1, y + seat_h / 2 + 1};
            out[seat].angle_milli = (int)(angle * 1000.0);
            out[seat].radius = row;
        }
    }
    return seat;
}

static void sort_chamber_slots(CouncilChamberSlot *slots, int count) {
    int i;
    for (i = 1; i < count; i++) {
        CouncilChamberSlot s = slots[i];
        int j = i - 1;
        while (j >= 0 && (slots[j].angle_milli > s.angle_milli ||
               (slots[j].angle_milli == s.angle_milli && slots[j].radius > s.radius))) {
            slots[j + 1] = slots[j];
            j--;
        }
        slots[j + 1] = s;
    }
}

int alliance_council_build_chamber_slots(RECT chart, RECT *out, int out_cap) {
    CouncilChamberSlot slots[ALLIANCE_COUNCIL_DISPLAY_SEATS];
    int i, count = build_chamber_slot_meta(chart, slots, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    if (!out || out_cap <= 0) return 0;
    sort_chamber_slots(slots, count);
    count = min(count, out_cap);
    for (i = 0; i < count; i++) out[i] = slots[i].rect;
    return count;
}

int alliance_council_build_visual_seats(const AllianceSnapshotRecord *record, RECT chart,
                                        AllianceCouncilSeatVisual *out, int out_cap) {
    AllianceCouncilDisplayMember members[MAX_CIVS];
    CouncilChamberSlot slots[ALLIANCE_COUNCIL_DISPLAY_SEATS];
    int i, n, count, slot_count, target, seat = 0;
    if (!out || out_cap <= 0) return 0;
    slot_count = build_chamber_slot_meta(chart, slots, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    sort_chamber_slots(slots, slot_count);
    target = min(min(out_cap, slot_count), ALLIANCE_COUNCIL_DISPLAY_SEATS);
    count = alliance_council_build_display_members(record, members, MAX_CIVS);
    for (i = 0; i < count; i++) {
        for (n = 0; n < members[i].display_seats && seat < out_cap && seat < target; n++, seat++) {
            out[seat].member_civ = members[i].member_civ;
            out[seat].rect = slots[seat].rect;
        }
    }
    return seat;
}

static void draw_round_box(HDC hdc, RECT r, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, 8, 8);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static int mix_channel(int a, int b, int amount) {
    return a + (b - a) * amount / 255;
}

static COLORREF mix_color(COLORREF color, COLORREF target, int amount) {
    return RGB(mix_channel(GetRValue(color), GetRValue(target), amount),
               mix_channel(GetGValue(color), GetGValue(target), amount),
               mix_channel(GetBValue(color), GetBValue(target), amount));
}

static COLORREF council_seat_display_color(COLORREF color) {
    int r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
    int maxc = max(r, max(g, b));
    int lum = (r * 30 + g * 59 + b * 11) / 100;
    if (maxc <= 0) return RGB(78, 96, 112);
    if (maxc < 116) {
        r = min(255, r * 116 / maxc);
        g = min(255, g * 116 / maxc);
        b = min(255, b * 116 / maxc);
        color = RGB(r, g, b);
        lum = (r * 30 + g * 59 + b * 11) / 100;
    }
    if (lum < 70) color = mix_color(color, RGB(255, 255, 255), min(80, (70 - lum) * 3));
    return color;
}

static void draw_seat_ellipse(HDC hdc, RECT r, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    Ellipse(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_council_seat(HDC hdc, RECT r, COLORREF color) {
    RECT shadow = {r.left + 1, r.top + 1, r.right + 1, r.bottom + 1};
    RECT face = {r.left + 1, r.top + 1, r.right - 1, r.bottom - 1};
    RECT glow = {face.left + 2, face.top + 2, face.right - 3, face.top + 6};
    draw_seat_ellipse(hdc, shadow, RGB(9, 20, 24), RGB(9, 20, 24));
    draw_seat_ellipse(hdc, r, mix_color(color, RGB(0, 0, 0), 40), RGB(6, 17, 21));
    draw_seat_ellipse(hdc, face, color, mix_color(color, RGB(0, 0, 0), 24));
    if (glow.right > glow.left && glow.bottom > glow.top)
        draw_seat_ellipse(hdc, glow, mix_color(color, RGB(255, 255, 255), 45), mix_color(color, RGB(255, 255, 255), 20));
}

static void draw_council_chip(HDC hdc, RECT r, const char *label, const char *value, COLORREF accent) {
    char text[96];
    snprintf(text, sizeof(text), "%s  %s", label, value);
    draw_round_box(hdc, r, RGB(26, 38, 42), RGB(57, 78, 84));
    fill_rect(hdc, (RECT){r.left + 7, r.top + 6, r.left + 11, r.bottom - 6}, accent);
    draw_text_rect(hdc, (RECT){r.left + 15, r.top, r.right - 7, r.bottom}, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_council_dais(HDC hdc, RECT chamber, int bottom_y) {
    int cx = (chamber.left + chamber.right) / 2;
    int cy = bottom_y - 17;
    COLORREF gold = RGB(218, 172, 82);
    POINT roof[3] = {{cx, cy - 5}, {cx - 17, cy + 2}, {cx + 17, cy + 2}};
    HBRUSH brush = CreateSolidBrush(gold);
    HGDIOBJ old = SelectObject(hdc, brush);
    Polygon(hdc, roof, 3);
    SelectObject(hdc, old);
    DeleteObject(brush);
    fill_rect(hdc, (RECT){cx - 13, cy + 3, cx + 13, cy + 6}, gold);
    fill_rect(hdc, (RECT){cx - 11, cy + 7, cx - 8, cy + 13}, gold);
    fill_rect(hdc, (RECT){cx - 2, cy + 7, cx + 2, cy + 13}, gold);
    fill_rect(hdc, (RECT){cx + 8, cy + 7, cx + 11, cy + 13}, gold);
    fill_rect(hdc, (RECT){cx - 17, cy + 14, cx + 17, cy + 17}, gold);
}

static void draw_percent_text(HDC hdc, RECT rect, int permille) {
    char text[32];
    snprintf(text, sizeof(text), "%d.%d%%", permille / 10, permille % 10);
    draw_text_rect(hdc, rect, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_member_row(HDC hdc, RECT r, const RenderSnapshot *snapshot,
                            const AllianceCouncilDisplayMember *member, int row_index) {
    RECT chip = {r.left + 6, r.top + 3, r.left + 116, r.bottom - 3};
    char text[96];
    if (!snapshot || !member || member->member_civ < 0 || member->member_civ >= snapshot->civ_count) return;
    fill_rect(hdc, r, row_index % 2 ? RGB(20, 36, 40) : RGB(18, 32, 36));
    fill_rect(hdc, chip, snapshot->civs[member->member_civ].color);
    draw_text_rect(hdc, chip, civ_name(snapshot, member->member_civ),
                   readable_text_color(snapshot->civs[member->member_civ].color),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d / %d", member->display_seats, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    draw_text_rect(hdc, (RECT){r.left + 126, r.top, r.left + 202, r.bottom}, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d.%d%%", member->display_seats * 1000 / ALLIANCE_COUNCIL_DISPLAY_SEATS / 10,
             member->display_seats * 1000 / ALLIANCE_COUNCIL_DISPLAY_SEATS % 10);
    draw_text_rect(hdc, (RECT){r.left + 204, r.top, r.left + 258, r.bottom}, text,
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_percent_text(hdc, (RECT){r.left + 260, r.top, r.left + 326, r.bottom}, member->population_permille);
    draw_percent_text(hdc, (RECT){r.left + 328, r.top, r.right - 4, r.bottom}, member->province_permille);
}

int alliance_council_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row ? row->alliance_id : -1);
    AllianceCouncilDisplayMember members[MAX_CIVS];
    int count = alliance_council_build_display_members(record, members, MAX_CIVS);
    return 278 + min(count, 6) * 24;
}

void alliance_council_draw_overview(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row ? row->alliance_id : -1);
    AllianceCouncilSeatVisual seats[ALLIANCE_COUNCIL_DISPLAY_SEATS];
    AllianceCouncilDisplayMember members[MAX_CIVS];
    RECT slots[128], card, inner, chips, chamber, seat_chart, header;
    char text[64], next_text[32];
    int i, slot_count, seat_count, count, visible_rows, seat_bottom;
    ui_section(hdc, cursor, tr("Alliance Council", "联盟议会"));
    count = alliance_council_build_display_members(record, members, MAX_CIVS);
    visible_rows = min(count, 6);
    card = ui_take_rect(cursor, 256 + visible_rows * 24);
    ui_clay_draw_card(hdc, card, UI_CLAY_STATE_NORMAL);
    inner = (RECT){card.left + 8, card.top + 8, card.right - 8, card.bottom - 8};
    chips = (RECT){inner.left, inner.top, inner.right, inner.top + 27};
    snprintf(next_text, sizeof(next_text), "%d%s",
             record ? max(0, record->council_next_election_year - (snapshot ? snapshot->year : 0)) : 0,
             ui_language == UI_LANG_ZH ? "年" : "y");
    {
        int gap = 6, total_w = chips.right - chips.left - gap * 2;
        int w = total_w * 31 / 100;
        int x2 = chips.left + w + gap;
        int x3 = x2 + w + gap;
        draw_council_chip(hdc, (RECT){chips.left, chips.top, chips.left + w, chips.bottom},
                          tr("Total votes", "总票数"), "80", RGB(105, 120, 210));
        snprintf(text, sizeof(text), "%d", alliance_council_display_threshold_two_thirds());
        draw_council_chip(hdc, (RECT){x2, chips.top, x2 + w, chips.bottom},
                          tr("2/3 threshold", "2/3门槛"), text, RGB(204, 166, 88));
        draw_council_chip(hdc, (RECT){x3, chips.top, chips.right, chips.bottom},
                          tr("Next election", "下次选举"), next_text, RGB(114, 157, 174));
    }
    chamber = (RECT){inner.left, chips.bottom + 8, inner.right, chips.bottom + 230};
    seat_chart = (RECT){chamber.left, chamber.top, chamber.right, chamber.top + 300};
    slot_count = alliance_council_build_chamber_slots(seat_chart, slots, 128);
    seat_bottom = chamber.top + 160;
    for (i = 0; i < slot_count; i++) seat_bottom = max(seat_bottom, slots[i].bottom);
    chamber.bottom = seat_bottom + 8;
    draw_round_box(hdc, chamber, RGB(17, 31, 36), RGB(42, 64, 70));
    for (i = 0; i < slot_count; i++) draw_council_seat(hdc, slots[i], RGB(25, 46, 50));
    seat_count = alliance_council_build_visual_seats(record, seat_chart, seats, ALLIANCE_COUNCIL_DISPLAY_SEATS);
    for (i = 0; i < seat_count; i++) {
        int member = seats[i].member_civ;
        if (member >= 0 && member < snapshot->civ_count)
            draw_council_seat(hdc, seats[i].rect, council_seat_display_color(snapshot->civs[member].color));
    }
    draw_council_dais(hdc, chamber, seat_bottom);
    header = (RECT){inner.left, chamber.bottom + 8, inner.right, chamber.bottom + 28};
    fill_rect(hdc, header, RGB(23, 42, 47));
    draw_text_rect(hdc, (RECT){header.left + 6, header.top, header.left + 124, header.bottom},
                   tr("Country", "国家"), ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){header.left + 126, header.top, header.left + 202, header.bottom},
                   tr("Votes", "票数"), ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){header.left + 204, header.top, header.left + 258, header.bottom},
                   tr("Share", "占比"), ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){header.left + 260, header.top, header.left + 326, header.bottom},
                   tr("Population", "人口"), ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){header.left + 328, header.top, header.right - 4, header.bottom},
                   tr("Provinces", "省份"), ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; i < visible_rows; i++) {
        RECT r = {inner.left, header.bottom + i * 24, inner.right, header.bottom + i * 24 + 22};
        draw_member_row(hdc, r, snapshot, &members[i], i);
    }
}
