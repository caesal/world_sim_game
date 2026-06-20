#include "game/game_diplomacy_visual_probe.h"

#include "core/render_snapshot.h"
#include "render/map_highlight.h"
#include "render/panel_country_actions.h"
#include "render/panel_country_diplomacy.h"
#include "render/panel_country_diplomacy_cards.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
#include "ui/ui_types.h"

#include <string.h>
#include <windows.h>

#define DIPLOMACY_PROBE_DIR "build/validation/diplomacy_alliance_probe_20260613"

static int write_bmp_from_bits(const char *path, BITMAPINFO *info, void *bits, int width, int height) {
    BITMAPFILEHEADER file_header;
    FILE *file;
    DWORD image_size = (DWORD)(width * height * 4);
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + image_size;
    file = fopen(path, "wb");
    if (!file) return 0;
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, image_size, 1, file);
    fclose(file);
    return 1;
}

static void fill_snapshot_civ(RenderSnapshot *snapshot, int id, const char *en, const char *zh, Color32 color) {
    snapshot->civs[id].alive = 1;
    snapshot->civs[id].id = id;
    snapshot->civs[id].uid = 1001 + id;
    snapshot->civs[id].symbol = (char)('A' + id);
    snapshot->civs[id].color = color;
    snapshot->civs[id].overlord = -1;
    snprintf(snapshot->civs[id].name_en, sizeof(snapshot->civs[id].name_en), "%s", en);
    snprintf(snapshot->civs[id].name_zh, sizeof(snapshot->civs[id].name_zh), "%s", zh);
}

static void fill_alliance_relation(SnapshotDiplomacyRelation *relation) {
    memset(relation, 0, sizeof(*relation));
    relation->state = DIPLOMACY_ALLIANCE;
    relation->relation_score = 92;
    relation->border_tension = 12;
    relation->trade_fit = 66;
    relation->resource_conflict = 5;
    relation->contact_kind = DIP_CONTACT_LAND_BORDER;
    relation->last_war_winner = -1;
    relation->last_war_loser = -1;
    relation->yearly_delta_x100 = 310;
    relation->state_years = 3;
    relation->candidate_state = DIPLOMACY_NONE;
    relation->relation_factor_ids[0] = DIP_REL_FACTOR_ALLIANCE;
    relation->relation_factor_delta_x100[0] = 100;
    relation->relation_factor_values[0] = 1;
    relation->relation_factor_ids[1] = DIP_REL_FACTOR_TRADE;
    relation->relation_factor_delta_x100[1] = 200;
    relation->relation_factor_values[1] = 66;
    relation->relation_factor_ids[2] = DIP_REL_FACTOR_HERITAGE;
    relation->relation_factor_delta_x100[2] = 10;
    relation->relation_factor_values[2] = 15;
}

static SnapshotDiplomacyRelation tab_relation(int state, int score, int tension, int conflict,
                                              int candidate, int candidate_years, int state_years) {
    SnapshotDiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = state;
    relation.relation_score = score;
    relation.border_tension = tension;
    relation.trade_fit = score > 0 ? 62 : 22;
    relation.resource_conflict = conflict;
    relation.contact_kind = DIP_CONTACT_LAND_BORDER;
    relation.years_known = 32;
    relation.state_years = state_years;
    relation.candidate_state = candidate;
    relation.candidate_years = candidate_years;
    relation.last_war_winner = -1;
    relation.last_war_loser = -1;
    if (state == DIPLOMACY_TRUCE) {
        relation.truce_years_left = 8;
        relation.truce_initial_years = 25;
        relation.last_war_result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    }
    relation.yearly_delta_x100 = score > 0 ? 150 : -100;
    relation.relation_factor_ids[0] = DIP_REL_FACTOR_CONTACT;
    relation.relation_factor_delta_x100[0] = 50;
    relation.relation_factor_values[0] = DIP_CONTACT_LAND_BORDER;
    relation.relation_factor_ids[1] = score > 0 ? DIP_REL_FACTOR_TRADE : DIP_REL_FACTOR_BORDER;
    relation.relation_factor_delta_x100[1] = score > 0 ? 100 : -150;
    relation.relation_factor_values[1] = score > 0 ? relation.trade_fit : tension;
    relation.relation_factor_ids[2] = conflict >= 70 ? DIP_REL_FACTOR_RESOURCE : DIP_REL_FACTOR_NONE;
    relation.relation_factor_delta_x100[2] = conflict >= 70 ? -200 : 0;
    relation.relation_factor_values[2] = conflict >= 70 ? conflict : 0;
    return relation;
}

static void fill_diplomacy_tab_snapshot(RenderSnapshot *snapshot) {
    int i;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->civ_count = 6;
    for (i = 0; i < snapshot->civ_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Diplomacy Probe %c", 'A' + i);
        fill_snapshot_civ(snapshot, i, name, name, COLOR32_RGB(78 + i * 20, 112 + i * 9, 150 + i * 5));
        snapshot->civs[i].current_soldiers = 1200 + i * 250;
        snapshot->civs[i].war_available_reserve = 700 + i * 100;
    }
    snapshot->civs[5].overlord = 0;
    snapshot->civs[5].vassal_callable_soldiers = 450;
    snapshot->civs[5].vassal_resource_tribute = 12;
    snapshot->relations[0][1] = tab_relation(DIPLOMACY_ALLIANCE, 91, 18, 8, DIPLOMACY_NONE, 0, 4);
    snapshot->relations[1][0] = tab_relation(DIPLOMACY_ALLIANCE, 88, 18, 8, DIPLOMACY_NONE, 0, 4);
    snapshot->relations[0][2] = tab_relation(DIPLOMACY_PEACE, 84, 12, 6, DIPLOMACY_ALLIANCE, 3, 14);
    snapshot->relations[2][0] = tab_relation(DIPLOMACY_PEACE, 82, 12, 6, DIPLOMACY_ALLIANCE, 3, 14);
    snapshot->relations[0][3] = tab_relation(DIPLOMACY_TRUCE, -24, 86, 30, DIPLOMACY_NONE, 0, 13);
    snapshot->relations[3][0] = tab_relation(DIPLOMACY_TRUCE, -28, 86, 30, DIPLOMACY_NONE, 0, 13);
    snapshot->relations[0][4] = tab_relation(DIPLOMACY_WAR, -82, 92, 82, DIPLOMACY_NONE, 0, 2);
    snapshot->relations[4][0] = tab_relation(DIPLOMACY_WAR, -78, 92, 82, DIPLOMACY_NONE, 0, 2);
    snapshot->relations[0][5] = tab_relation(DIPLOMACY_VASSAL, 40, 10, 5, DIPLOMACY_NONE, 0, 8);
    snapshot->relations[5][0] = snapshot->relations[0][5];
    snapshot->wars[0][4].active = snapshot->wars[4][0].active = 1;
    snapshot->wars[0][4].attacker = snapshot->wars[4][0].attacker = 0;
    snapshot->wars[0][4].defender = snapshot->wars[4][0].defender = 4;
    snapshot->wars[0][4].soldiers_a = snapshot->wars[4][0].soldiers_a = 1800;
    snapshot->wars[0][4].soldiers_b = snapshot->wars[4][0].soldiers_b = 1550;
}

static int render_alliance_card_bmp(const char *path, int language, int tooltip_variant) {
    const int width = 420;
    const int height = tooltip_variant ? 410 : 250;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    HFONT font = NULL;
    RECT client = {0, 0, width, height};
    HBRUSH brush;
    UiCursor cursor;
    static RenderSnapshot snapshot;
    SnapshotDiplomacyRelation relation;
    int old_hover_x = hover_x, old_hover_y = hover_y;
    int ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !mem) goto cleanup;
    old_bitmap = SelectObject(mem, bitmap);
    brush = CreateSolidBrush(RGB(244, 242, 236));
    FillRect(mem, &client, brush);
    DeleteObject(brush);
    font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, language ? L"Microsoft YaHei UI" : L"Segoe UI");
    SelectObject(mem, font);
    ui_language = language;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.world_generated = 1;
    snapshot.civ_count = 2;
    fill_snapshot_civ(&snapshot, 0, "Alliance Probe A", "同盟甲", COLOR32_RGB(84, 132, 190));
    fill_snapshot_civ(&snapshot, 1, "Alliance Probe B", "同盟乙", COLOR32_RGB(92, 164, 138));
    fill_alliance_relation(&relation);
    if (tooltip_variant == 1) {
        relation.yearly_delta_x100 = 310;
    } else if (tooltip_variant == 2) {
        relation.state = DIPLOMACY_TENSE;
        relation.relation_score = -32;
        relation.yearly_delta_x100 = -300;
        relation.relation_factor_ids[0] = DIP_REL_FACTOR_BORDER;
        relation.relation_factor_delta_x100[0] = -150;
        relation.relation_factor_values[0] = 86;
        relation.relation_factor_ids[1] = DIP_REL_FACTOR_RESOURCE;
        relation.relation_factor_delta_x100[1] = -200;
        relation.relation_factor_values[1] = 61;
        relation.relation_factor_ids[2] = DIP_REL_FACTOR_CONTACT;
        relation.relation_factor_delta_x100[2] = 50;
        relation.relation_factor_values[2] = DIP_CONTACT_LAND_BORDER;
    } else if (tooltip_variant == 3) {
        relation.state = DIPLOMACY_PEACE;
        relation.relation_score = 0;
        relation.yearly_delta_x100 = 0;
        relation.relation_factor_ids[0] = DIP_REL_FACTOR_CONTACT;
        relation.relation_factor_delta_x100[0] = 50;
        relation.relation_factor_values[0] = DIP_CONTACT_LAND_BORDER;
        relation.relation_factor_ids[1] = DIP_REL_FACTOR_BORDER;
        relation.relation_factor_delta_x100[1] = -50;
        relation.relation_factor_values[1] = 45;
        relation.relation_factor_ids[2] = DIP_REL_FACTOR_NONE;
        relation.relation_factor_delta_x100[2] = 0;
    }
    snapshot.relations[0][1] = relation;
    snapshot.relations[1][0] = relation;
    diplomacy_score_tooltip_begin();
    hover_x = tooltip_variant ? 80 : -10000;
    hover_y = tooltip_variant ? 92 : -10000;
    render_context_begin(&snapshot);
    cursor = ui_cursor(12, 12, width - 24, height - 24);
    draw_diplomacy_relation_card(mem, &cursor, 0, 1, DIPLOMACY_VIEW_ALLIANCE);
    if (tooltip_variant) diplomacy_score_tooltip_draw(mem, client);
    render_context_end();
    ok = write_bmp_from_bits(path, &info, bits, width, height);
cleanup:
    hover_x = old_hover_x;
    hover_y = old_hover_y;
    if (font) DeleteObject(font);
    if (bitmap) {
        SelectObject(mem, old_bitmap);
        DeleteObject(bitmap);
    }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static int render_diplomacy_tab_bmp(const char *path, int view, int scroll, int empty_group) {
    const int width = 560, height = 760;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    HFONT font = NULL;
    RECT client = {0, 0, width, height};
    HBRUSH brush;
    UiCursor cursor;
    static RenderSnapshot snapshot;
    int old_selected = selected_civ, old_view = country_diplomacy_view;
    int ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !mem) goto cleanup;
    old_bitmap = SelectObject(mem, bitmap);
    brush = CreateSolidBrush(RGB(244, 242, 236));
    FillRect(mem, &client, brush);
    DeleteObject(brush);
    font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SelectObject(mem, font);
    selected_civ = 0;
    country_diplomacy_view = view;
    fill_diplomacy_tab_snapshot(&snapshot);
    if (empty_group) {
        snapshot.relations[0][2].state = DIPLOMACY_TENSE;
        snapshot.relations[2][0].state = DIPLOMACY_TENSE;
    }
    render_context_begin(&snapshot);
    cursor = ui_cursor(16, 16, width - 32, height - 32);
    draw_country_diplomacy_tab(mem, &cursor, client, scroll, 0);
    render_context_end();
    ok = write_bmp_from_bits(path, &info, bits, width, height);
cleanup:
    selected_civ = old_selected;
    country_diplomacy_view = old_view;
    if (font) DeleteObject(font);
    if (bitmap) {
        SelectObject(mem, old_bitmap);
        DeleteObject(bitmap);
    }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static int render_actions_bmp(const char *path) {
    const int width = 420, height = 190;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    HFONT font = NULL;
    RECT client = {0, 0, width, height};
    HBRUSH brush;
    UiCursor cursor;
    static RenderSnapshot snapshot;
    int ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !mem) goto cleanup;
    old_bitmap = SelectObject(mem, bitmap);
    brush = CreateSolidBrush(RGB(244, 242, 236));
    FillRect(mem, &client, brush);
    DeleteObject(brush);
    font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SelectObject(mem, font);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.world_generated = 1;
    snapshot.civ_count = 1;
    fill_snapshot_civ(&snapshot, 0, "Operation Probe", "Operation Probe", COLOR32_RGB(110, 150, 190));
    snapshot.civs[0].collapse_can_trigger = 1;
    render_context_begin(&snapshot);
    cursor = ui_cursor(14, 14, width - 28, height - 28);
    draw_country_overview_actions(mem, &cursor, 0);
    render_context_end();
    ok = write_bmp_from_bits(path, &info, bits, width, height);
cleanup:
    if (font) DeleteObject(font);
    if (bitmap) {
        SelectObject(mem, old_bitmap);
        DeleteObject(bitmap);
    }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static void setup_highlight_snapshot_civ(RenderSnapshot *snapshot, int id,
                                         const char *name, Color32 color, int focus_x) {
    snapshot->civs[id].alive = 1;
    snapshot->civs[id].id = id;
    snapshot->civs[id].uid = 2000 + id;
    snapshot->civs[id].symbol = (char)('A' + id);
    snapshot->civs[id].color = color;
    snapshot->civs[id].overlord = -1;
    snapshot->civs[id].focus_x = focus_x;
    snapshot->civs[id].focus_y = 5;
    snapshot->civs[id].focus_valid = 1;
    snprintf(snapshot->civs[id].name_en, sizeof(snapshot->civs[id].name_en), "%s", name);
    snprintf(snapshot->civs[id].name_zh, sizeof(snapshot->civs[id].name_zh), "%s", name);
}

static int render_alliance_highlight_bmp(const char *path) {
    const int width = 880, height = 220;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    RECT client = {0, 0, width, height};
    MapLayout layout = {20, 56, 4, MAX_CIVS * 4, 100};
    HBRUSH brush;
    static RenderSnapshot snapshot;
    int old_selected = selected_civ, old_highlight = map_highlight_civ;
    int old_pulse = selected_civ_pulse_start_ms, old_collapsed = side_panel_collapsed;
    int x, y, owner, ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !mem) goto cleanup;
    old_bitmap = SelectObject(mem, bitmap);
    brush = CreateSolidBrush(RGB(32, 36, 38));
    FillRect(mem, &client, brush);
    DeleteObject(brush);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.world_generated = 1;
    snapshot.map_w = MAX_CIVS;
    snapshot.map_h = 10;
    snapshot.civ_count = MAX_CIVS;
    snapshot.tiles_revision = 77;
    for (owner = 0; owner < MAX_CIVS; owner++) {
        setup_highlight_snapshot_civ(&snapshot, owner, "Relation Probe", COLOR32_RGB(86, 96, 104), owner);
        if (owner > 0) {
            snapshot.relations[0][owner].state = DIPLOMACY_ALLIANCE;
            snapshot.relations[owner][0].state = DIPLOMACY_ALLIANCE;
        }
    }
    snapshot.civs[0].color = COLOR32_RGB(128, 150, 92);
    snapshot.civs[3].overlord = 0;
    snapshot.wars[0][2].active = snapshot.wars[2][0].active = 1;
    for (y = 0; y < snapshot.map_h; y++) {
        for (x = 0; x < snapshot.map_w; x++) {
            owner = x;
            snapshot.tiles[y * snapshot.map_w + x].owner = (short)owner;
            snapshot.tiles[y * snapshot.map_w + x].geography = GEO_PLAIN;
            snapshot.tiles[y * snapshot.map_w + x].province_id = (short)owner;
            snapshot.tiles[y * snapshot.map_w + x].region_id = (short)owner;
        }
    }
    selected_civ = 0;
    map_highlight_civ = -1;
    selected_civ_pulse_start_ms = 0;
    side_panel_collapsed = 1;
    map_highlight_overlay_reset_debug();
    render_context_begin(&snapshot);
    draw_country_highlight(mem, client, layout);
    render_context_end();
    ok = write_bmp_from_bits(path, &info, bits, width, height);
cleanup:
    selected_civ = old_selected;
    map_highlight_civ = old_highlight;
    selected_civ_pulse_start_ms = old_pulse;
    side_panel_collapsed = old_collapsed;
    if (bitmap) {
        SelectObject(mem, old_bitmap);
        DeleteObject(bitmap);
    }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static int case_alliance_card_render(FILE *summary) {
    int en_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/alliance_card_en.bmp", UI_LANG_EN, 0);
    int zh_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/alliance_card_zh.bmp", UI_LANG_ZH, 0);
    int tooltip_pos_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/tooltip_positive_residual_en.bmp", UI_LANG_EN, 1);
    int tooltip_neg_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/tooltip_negative_en.bmp", UI_LANG_EN, 2);
    int tooltip_zero_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/tooltip_zero_en.bmp", UI_LANG_EN, 3);
    int tooltip_zh_ok = render_alliance_card_bmp(DIPLOMACY_PROBE_DIR "/tooltip_positive_residual_zh.bmp", UI_LANG_ZH, 1);
    ui_language = UI_LANG_EN;
    fprintf(summary,
            "case=alliance_card_render en=%d zh=%d tooltip_pos=%d tooltip_neg=%d tooltip_zero=%d tooltip_zh=%d files=alliance_card_en.bmp/alliance_card_zh.bmp/tooltip_positive_residual_en.bmp/tooltip_negative_en.bmp/tooltip_zero_en.bmp/tooltip_positive_residual_zh.bmp\n",
            en_ok, zh_ok, tooltip_pos_ok, tooltip_neg_ok, tooltip_zero_ok, tooltip_zh_ok);
    return en_ok && zh_ok && tooltip_pos_ok && tooltip_neg_ok && tooltip_zero_ok && tooltip_zh_ok;
}

static int case_alliance_highlight_render(FILE *summary) {
    int ok = render_alliance_highlight_bmp(DIPLOMACY_PROBE_DIR "/alliance_highlight_sample.bmp");
    int requests = map_highlight_overlay_last_requests();
    int selected = map_highlight_last_selected_request_present();
    int allies = map_highlight_last_alliance_request_count();
    int war = map_highlight_last_war_request_count();
    int vassal = map_highlight_last_vassal_request_count();
    int passed = ok && requests > 96 && selected && allies > 96 && war > 0 && vassal > 0;
    fprintf(summary,
            "case=alliance_highlight_render ok=%d requests=%d selected=%d allies=%d war=%d vassal=%d file=alliance_highlight_sample.bmp ally_color=RGB(86,152,218)\n",
            ok, requests, selected, allies, war, vassal);
    return passed;
}

static int case_diplomacy_tab_render(FILE *summary) {
    int ok = 1;
    int tense_ok, truce_hits;
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_alliance.bmp", DIPLOMACY_VIEW_ALLIANCE, 0, 0);
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_peace.bmp", DIPLOMACY_VIEW_PEACE, 0, 0);
    tense_ok = render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_tense.bmp", DIPLOMACY_VIEW_TENSE, 0, 0);
    truce_hits = diplomacy_score_tooltip_registered_count();
    ok &= tense_ok;
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_war.bmp", DIPLOMACY_VIEW_WAR, 0, 0);
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_vassal.bmp", DIPLOMACY_VIEW_VASSAL, 0, 0);
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_peace_scrolled.bmp", DIPLOMACY_VIEW_PEACE, 180, 0);
    ok &= render_diplomacy_tab_bmp(DIPLOMACY_PROBE_DIR "/tab_peace_empty.bmp", DIPLOMACY_VIEW_PEACE, 0, 1);
    fprintf(summary, "case=diplomacy_tab_render ok=%d truce_score_tooltip_hits=%d files=tab_alliance.bmp/tab_peace.bmp/tab_tense.bmp/tab_war.bmp/tab_vassal.bmp/tab_peace_scrolled.bmp/tab_peace_empty.bmp\n", ok, truce_hits);
    return ok && truce_hits > 0;
}

static int case_actions_render(FILE *summary) {
    int ok = render_actions_bmp(DIPLOMACY_PROBE_DIR "/country_actions_buttons.bmp");
    fprintf(summary, "case=country_actions_render ok=%d file=country_actions_buttons.bmp buttons=DeclareWar/Peace/Alliance/Dissolve/Vassalize/CivilUnrest\n", ok);
    return ok;
}

static int case_tooltip_factor_accounting(FILE *summary) {
    static RenderSnapshot snapshot;
    int sum = 0, other = 0, display = 0;
    int ok;
    fill_diplomacy_tab_snapshot(&snapshot);
    render_context_begin(&snapshot);
    ok = diplomacy_score_tooltip_net_for_relation(0, 2, &sum, &other, &display);
    render_context_end();
    fprintf(summary,
            "case=tooltip_factor_accounting ok=%d factor_sum_x100=%d other_x100=%d display_x100=%d label=\"+1.5/y\"\n",
            ok, sum, other, display);
    return ok && display == 150 && sum + other == display;
}

int run_diplomacy_visual_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_alliance_card_render(summary);
    ok &= case_alliance_highlight_render(summary);
    ok &= case_diplomacy_tab_render(summary);
    ok &= case_actions_render(summary);
    ok &= case_tooltip_factor_accounting(summary);
    return ok;
}
