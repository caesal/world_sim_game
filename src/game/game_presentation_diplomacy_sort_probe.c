#include "core/render_snapshot.h"
#include "render/panel_country_diplomacy.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER header;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&header, 0, sizeof(header));
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static void set_civ(RenderSnapshot *s, int id, int population) {
    SnapshotCiv *civ = &s->civs[id];
    civ->alive = 1;
    civ->id = id;
    civ->uid = 9000 + id;
    civ->symbol = (char)('A' + id);
    civ->population = population;
    civ->summary.population = population;
    civ->color = RGB(80 + id * 13, 110 + id * 7, 150 + id * 5);
    civ->overlord = -1;
    snprintf(civ->name_en, sizeof(civ->name_en), "Sort Civ %d", id);
    snprintf(civ->name_zh, sizeof(civ->name_zh), "Sort Civ %d", id);
}

static void set_relation(RenderSnapshot *s, int other_id, int state,
                         int truce_years, int tension) {
    SnapshotDiplomacyRelation rel;
    memset(&rel, 0, sizeof(rel));
    rel.state = state;
    rel.truce_years_left = truce_years;
    rel.truce_initial_years = truce_years;
    rel.border_tension = tension;
    rel.contact_kind = DIP_CONTACT_LAND_BORDER;
    rel.relation_score = state == DIPLOMACY_TRUCE ? -20 : -60;
    s->relations[0][other_id] = rel;
    s->relations[other_id][0] = rel;
}

static void fill_sort_snapshot(RenderSnapshot *s) {
    int i;
    memset(s, 0, sizeof(*s));
    s->world_generated = 1;
    s->year = 80;
    s->month = 4;
    s->map_w = 64;
    s->map_h = 36;
    s->civ_count = 10;
    for (i = 0; i < s->civ_count; i++) set_civ(s, i, 100 + i * 10);
    set_civ(s, 1, 1000);
    set_civ(s, 2, 1000);
    set_civ(s, 3, 900);
    set_civ(s, 4, 1400);
    set_civ(s, 5, 1400);
    set_civ(s, 6, 1000);
    set_civ(s, 7, 900);
    set_civ(s, 8, 1500);
    set_civ(s, 9, 1500);
    set_relation(s, 1, DIPLOMACY_TRUCE, 52, 0);
    set_relation(s, 2, DIPLOMACY_TRUCE, 42, 0);
    set_relation(s, 3, DIPLOMACY_TRUCE, 22, 0);
    set_relation(s, 4, DIPLOMACY_TRUCE, 22, 0);
    set_relation(s, 5, DIPLOMACY_TRUCE, 22, 0);
    set_relation(s, 6, DIPLOMACY_TENSE, 0, 90);
    set_relation(s, 7, DIPLOMACY_TENSE, 0, 80);
    set_relation(s, 8, DIPLOMACY_TENSE, 0, 80);
    set_relation(s, 9, DIPLOMACY_TENSE, 0, 80);
}

static int render_sort_artifact(void) {
    const int w = 900, h = 720;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT viewport = {24, 24, w - 24, h - 24};
    UiCursor cursor = ui_cursor(viewport.left, viewport.top, viewport.right - viewport.left, viewport.bottom);
    int old_selected = selected_civ;
    int old_subtab = country_detail_subtab;
    int old_view = country_diplomacy_view;
    int old_language = ui_language;
    int ok;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, (RECT){0, 0, w, h}, RGB(25, 30, 33));
    selected_civ = 0;
    country_detail_subtab = COUNTRY_DETAIL_DIPLOMACY;
    country_diplomacy_view = DIPLOMACY_VIEW_TENSE;
    ui_language = UI_LANG_EN;
    draw_country_diplomacy_tab(hdc, &cursor, viewport, 0, 0);
    ok = write_bmp(PRESENTATION_PROBE_DIR "/diplomacy_tense_sort_order.bmp",
                   &info, bits, w, h);
    selected_civ = old_selected;
    country_detail_subtab = old_subtab;
    country_diplomacy_view = old_view;
    ui_language = old_language;
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

int game_presentation_diplomacy_sort_probe(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int ids[MAX_CIVS];
    int count;
    int artifact;
    int truce_order, truce_tie_population, tense_order, tense_tie_population, civ_id_tie;
    if (!snapshot) {
        fprintf(summary, "case=diplomacy_tense_sort_order ok=0 reason=alloc\n");
        return 0;
    }
    fill_sort_snapshot(snapshot);
    render_context_begin(snapshot);
    count = panel_country_diplomacy_probe_collect_tense_order(0, ids, MAX_CIVS);
    artifact = render_sort_artifact();
    render_context_end();
    truce_order = count >= 9 && ids[0] == 1 && ids[1] == 2 &&
                  ids[2] == 4 && ids[3] == 5 && ids[4] == 3;
    truce_tie_population = count >= 5 && ids[2] == 4 && ids[4] == 3;
    tense_order = count >= 9 && ids[5] == 6 && ids[6] == 8 &&
                  ids[7] == 9 && ids[8] == 7;
    tense_tie_population = count >= 9 && ids[6] == 8 && ids[8] == 7;
    civ_id_tie = count >= 8 && ids[2] == 4 && ids[3] == 5 && ids[6] == 8 && ids[7] == 9;
    fprintf(summary,
            "case=diplomacy_tense_sort_order ok=%d truce_order=52/42/22 truce_tie_population=%d tense_order=90/80 tense_tie_population=%d civ_id_tie=%d artifact=diplomacy_tense_sort_order.bmp\n",
            count == 9 && truce_order && truce_tie_population && tense_order &&
            tense_tie_population && civ_id_tie && artifact,
            truce_tie_population, tense_tie_population, civ_id_tie);
    free(snapshot);
    return count == 9 && truce_order && truce_tie_population && tense_order &&
           tense_tie_population && civ_id_tie && artifact;
}
