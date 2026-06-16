#include "game/game_diplomacy_tooltip_probe.h"

#include "core/render_snapshot.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static void tooltip_probe_civ(RenderSnapshot *snapshot, int id) {
    snapshot->civs[id].alive = 1;
    snapshot->civs[id].id = id;
    snapshot->civs[id].uid = 4000 + id;
    snapshot->civs[id].symbol = (char)('A' + (id % 26));
    snapshot->civs[id].color = COLOR32_RGB(72 + id % 80, 104 + id % 60, 138 + id % 50);
    snapshot->civs[id].overlord = -1;
    snprintf(snapshot->civs[id].name_en, sizeof(snapshot->civs[id].name_en),
             "Tooltip Probe %d", id);
    snprintf(snapshot->civs[id].name_zh, sizeof(snapshot->civs[id].name_zh),
             "Tooltip Probe %d", id);
}

static void tooltip_probe_relation(SnapshotDiplomacyRelation *relation, int score) {
    memset(relation, 0, sizeof(*relation));
    relation->state = DIPLOMACY_PEACE;
    relation->relation_score = score;
    relation->contact_kind = DIP_CONTACT_LAND_BORDER;
    relation->yearly_delta_x10 = 15;
    relation->relation_factor_ids[0] = DIP_REL_FACTOR_CONTACT;
    relation->relation_factor_delta_x10[0] = 5;
    relation->relation_factor_values[0] = DIP_CONTACT_LAND_BORDER;
    relation->relation_factor_ids[1] = DIP_REL_FACTOR_TRADE;
    relation->relation_factor_delta_x10[1] = 10;
    relation->relation_factor_values[1] = 62;
}

static void tooltip_probe_snapshot(RenderSnapshot *snapshot, int count) {
    int i;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->civ_count = count;
    for (i = 0; i < count; i++) tooltip_probe_civ(snapshot, i);
    for (i = 1; i < count; i++) {
        tooltip_probe_relation(&snapshot->relations[0][i], 55 + i % 40);
        tooltip_probe_relation(&snapshot->relations[i][0], 45 + i % 35);
    }
}

static int case_late_tooltip_hit(FILE *summary) {
    static RenderSnapshot snapshot;
    int count = 100;
    int late = 99;
    int i, first_civ = -1, first_other = -1, hit_civ = -1, hit_other = -1;
    int sum = 0, other = 0, display = 0;
    int first_ok, hit_ok, key_ok;
    tooltip_probe_snapshot(&snapshot, count);
    render_context_begin(&snapshot);
    diplomacy_score_tooltip_begin();
    for (i = 1; i < count; i++) {
        RECT rect = {10, i * 56, 210, i * 56 + 52};
        diplomacy_score_tooltip_register_bar(rect, 0, i);
    }
    first_ok = diplomacy_score_tooltip_hit_test(20, 1 * 56 + 8, &first_civ, &first_other);
    hit_ok = diplomacy_score_tooltip_hit_test(20, late * 56 + 8, &hit_civ, &hit_other);
    key_ok = diplomacy_score_tooltip_hover_key(20, late * 56 + 8) == 1 + late;
    diplomacy_score_tooltip_net_for_relation(hit_civ, hit_other, &sum, &other, &display);
    render_context_end();
    fprintf(summary,
            "case=late_tooltip_hit registered=%d first=%d/%d late=%d/%d key=%d summary_region=1 sum=%d other_delta=%d display=%d\n",
            diplomacy_score_tooltip_registered_count(), first_civ, first_other,
            hit_civ, hit_other, key_ok, sum, other, display);
    return diplomacy_score_tooltip_registered_count() == count - 1 &&
           first_ok && first_civ == 0 && first_other == 1 &&
           hit_ok && key_ok && hit_civ == 0 && hit_other == late &&
           sum + other == display && display == 15;
}

static void fill_probe_pixels(unsigned int *pixels, int count, unsigned int color) {
    int i;
    for (i = 0; i < count; i++) pixels[i] = color;
}

static int changed_probe_pixels(unsigned int *pixels, int count, unsigned int color) {
    int i, changed = 0;
    for (i = 0; i < count; i++) {
        if (pixels[i] != color) changed++;
    }
    return changed;
}

static int case_overlay_stable_after_base(FILE *summary) {
    static RenderSnapshot snapshot;
    const int width = 360, height = 240;
    const unsigned int bg = 0x00202020u;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    RECT bounds = {0, 0, width, height};
    int old_hover_x = hover_x, old_hover_y = hover_y;
    int first_pixels = 0, second_pixels = 0, key_ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bitmap && bits && mem) {
        old_bitmap = SelectObject(mem, bitmap);
        tooltip_probe_snapshot(&snapshot, 3);
        render_context_begin(&snapshot);
        diplomacy_score_tooltip_begin();
        diplomacy_score_tooltip_register_bar((RECT){10, 10, 210, 62}, 0, 1);
        hover_x = 20;
        hover_y = 20;
        key_ok = diplomacy_score_tooltip_hover_key(hover_x, hover_y) > 0;
        fill_probe_pixels((unsigned int *)bits, width * height, bg);
        diplomacy_score_tooltip_draw(mem, bounds);
        first_pixels = changed_probe_pixels((unsigned int *)bits, width * height, bg);
        fill_probe_pixels((unsigned int *)bits, width * height, bg);
        diplomacy_score_tooltip_draw(mem, bounds);
        second_pixels = changed_probe_pixels((unsigned int *)bits, width * height, bg);
        render_context_end();
    }
    hover_x = old_hover_x;
    hover_y = old_hover_y;
    if (bitmap) {
        if (old_bitmap) SelectObject(mem, old_bitmap);
        DeleteObject(bitmap);
    }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    fprintf(summary,
            "case=tooltip_overlay_stable_after_base key=%d first_pixels=%d second_pixels=%d\n",
            key_ok, first_pixels, second_pixels);
    return key_ok && first_pixels > 256 && second_pixels == first_pixels;
}

int run_diplomacy_tooltip_probe_cases(FILE *summary) {
    return case_late_tooltip_hit(summary) &&
           case_overlay_stable_after_base(summary);
}
