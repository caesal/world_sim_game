#include "render/panel_map_speed_badge.h"

#include "core/constants.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/panel_debug.h"
#include "render/panel_view_model_cache.h"
#include "render/render_common.h"
#include "render/render_ocean_decoration.h"
#include "sim/decision_snapshot.h"
#include "ui/ui_alliance_panel_input.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"
#include "core/game_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER file_header;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static int text_width(const char *text) {
    HDC screen = GetDC(NULL);
    SIZE size;
    measure_text_utf8(screen, text, &size);
    ReleaseDC(NULL, screen);
    return size.cx;
}

static int render_speed_badge_bmp(const char *path, int language, int auto_running,
                                  int render_ms, int pending) {
    const int width = 1100, height = 420;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RECT map = get_map_viewport_rect(client);
    MapLayout layout = get_map_layout(client);
    RECT content = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w,
                    layout.map_y + layout.draw_h};
    RECT bottom = {0, height - BOTTOM_BAR_H, width, height};
    int ok;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, client, RGB(17, 24, 28));
    fill_rect(hdc, map, RGB(75, 151, 202));
    fill_rect(hdc, content, RGB(132, 154, 86));
    fill_rect(hdc, bottom, RGB(20, 26, 30));
    panel_map_draw_actual_speed_badge(hdc, client, 40);
    panel_map_draw_bottom_status_chips(hdc, client, 0, language, render_ms, pending, auto_running, 0);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

static void fill_ocean_probe_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->terrain_revision = 77;
    snapshot->region_count = 24;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int land_score = (x - 45) * (x - 45) / 11 + (y - 31) * (y - 31) / 7;
            int island = land_score < 38;
            int coastal = !island && land_score < 64;
            int lake = x > 10 && x < 18 && y > 46 && y < 54;
            int shallow_open = !island && !coastal && x > 56 && x < 94 && y > 5 && y < 28;
            tile->geography = island ? GEO_ISLAND : (lake ? GEO_LAKE : (coastal ? GEO_BAY : GEO_OCEAN));
            tile->climate = CLIMATE_OCEANIC;
            tile->water_depth = island ? WATER_DEPTH_NONE :
                                (coastal || lake || shallow_open ? WATER_DEPTH_SHALLOW : WATER_DEPTH_DEEP);
            tile->water_deep_percent = island ? 0 : (coastal || lake || shallow_open ? 35 : 86);
            tile->elevation = island ? 48 : 12;
            tile->owner = -1;
            tile->region_id = island ? 2 : -1;
            tile->province_id = -1;
        }
    }
    snapshot->lane_count = 1;
    snapshot->lanes[0].active = 1;
    snapshot->lanes[0].point_count = 2;
    snapshot->lanes[0].points[0] = (MapPoint){18, 18};
    snapshot->lanes[0].points[1] = (MapPoint){82, 45};
}

static int render_ocean_decoration_bmp(const char *path, int split, int zoomed) {
    const int width = 1100, height = 620;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RECT viewport;
    MapLayout layout;
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int ok, x, y, saved_dc;

    if (!snapshot) {
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = zoomed ? 165 : 100;
    map_offset_x = zoomed ? -95 : 0;
    map_offset_y = zoomed ? 48 : 0;
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);
    fill_ocean_probe_snapshot(snapshot);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, client, RGB(18, 24, 28));
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            RECT cell = {layout.map_x + x * layout.draw_w / snapshot->map_w,
                         layout.map_y + y * layout.draw_h / snapshot->map_h,
                         layout.map_x + (x + 1) * layout.draw_w / snapshot->map_w + 1,
                         layout.map_y + (y + 1) * layout.draw_h / snapshot->map_h + 1};
            fill_rect(hdc, cell, tile->water_depth == WATER_DEPTH_NONE ?
                      RGB(144, 174, 116) : RGB(54, 126, 184));
        }
    }
    RestoreDC(hdc, saved_dc);
    if (split) {
        RECT left = {viewport.left + 12, viewport.top + 12, viewport.left + 182, viewport.top + 36};
        RECT right = {left.right + 10, left.top, left.right + 190, left.bottom};
        fill_rect(hdc, left, RGB(40, 88, 94));
        fill_rect(hdc, right, RGB(96, 72, 86));
        draw_center_text(hdc, left, "40 / 80 water bloc", RGB(232, 238, 232));
        draw_center_text(hdc, right, "40 / 80 water bloc", RGB(232, 238, 232));
    }
    render_ocean_decoration_draw_overlay(hdc, client, layout, snapshot);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    free(snapshot);
    return ok;
}

static int case_ocean_decoration_layer(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 1100, 620);
    HBITMAP old = SelectObject(hdc, bitmap);
    RECT client = {0, 0, 1100, 620};
    RECT resized = {0, 0, 1140, 620};
    MapLayout layout;
    OceanDecorationProbeInfo a, b, c, d;
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int artifact_ok, ok;

    if (!snapshot) {
        SelectObject(hdc, old);
        DeleteObject(bitmap);
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    layout = get_map_layout(client);
    fill_ocean_probe_snapshot(snapshot);
    render_ocean_decoration_reset_debug();
    render_ocean_decoration_draw(hdc, client, layout, snapshot);
    a = render_ocean_decoration_probe_info();
    render_ocean_decoration_draw(hdc, client, layout, snapshot);
    b = render_ocean_decoration_probe_info();
    map_zoom_percent = 165;
    map_offset_x = -95;
    map_offset_y = 48;
    render_ocean_decoration_draw(hdc, client, get_map_layout(client), snapshot);
    d = render_ocean_decoration_probe_info();
    render_ocean_decoration_draw(hdc, resized, get_map_layout(resized), snapshot);
    c = render_ocean_decoration_probe_info();
    artifact_ok = render_ocean_decoration_bmp(PRESENTATION_PROBE_DIR "/ocean_decoration_full.bmp", 0, 0);
    artifact_ok &= render_ocean_decoration_bmp(PRESENTATION_PROBE_DIR "/ocean_decoration_split_40_80.bmp", 1, 0);
    artifact_ok &= render_ocean_decoration_bmp(PRESENTATION_PROBE_DIR "/ocean_decoration_zoom_pan.bmp", 0, 1);
    ok = a.exterior_items >= 12 && a.interior_items > 0 && a.compass_items == 0 &&
         a.interior_water_only && a.interior_deep_only && a.interior_shallow_allowed_seen &&
         a.same_type_spacing_ok && a.motif_overlap_count == 0 &&
         a.motif_mask != 0 && a.item_hash == b.item_hash && a.item_hash == d.item_hash &&
         a.item_hash == c.item_hash &&
         a.texture_asset_ready && a.motif_asset_ready && a.exterior_texture_score >= 800 &&
         a.interior_texture_score >= 800 &&
         a.primitive_wave_stamps == 0 &&
         a.interior_min_clearance >= 4 &&
         b.item_rebuilds == a.item_rebuilds && d.item_rebuilds == b.item_rebuilds &&
         c.item_rebuilds == b.item_rebuilds && d.exterior_rebuilds == b.exterior_rebuilds &&
         d.interior_rebuilds == b.interior_rebuilds && c.exterior_rebuilds > b.exterior_rebuilds &&
         artifact_ok;
    fprintf(summary,
            "case=ocean_decoration_layer ok=%d ext=%d int=%d item_rebuilds=%d/%d/%d/%d exterior_rebuilds=%d/%d/%d/%d interior_rebuilds=%d/%d/%d/%d hash=%u motif_mask=0x%x compass=%d water_only=%d deep_only=%d shallow_allowed_seen=%d same_type_spacing=%d overlaps=%d min_clearance=%d texture=%d/%d asset=%d motif_asset=%d primitive_waves=%d artifacts=%d files=ocean_decoration_full.bmp/ocean_decoration_split_40_80.bmp/ocean_decoration_zoom_pan.bmp\n",
            ok, a.exterior_items, a.interior_items, a.item_rebuilds, b.item_rebuilds,
            d.item_rebuilds, c.item_rebuilds, a.exterior_rebuilds, b.exterior_rebuilds,
            d.exterior_rebuilds, c.exterior_rebuilds, a.interior_rebuilds, b.interior_rebuilds,
            d.interior_rebuilds, c.interior_rebuilds,
            a.item_hash, a.motif_mask, a.compass_items, a.interior_water_only,
            a.interior_deep_only, a.interior_shallow_allowed_seen, a.same_type_spacing_ok,
            a.motif_overlap_count, a.interior_min_clearance,
            a.exterior_texture_score, a.interior_texture_score,
            a.texture_asset_ready, a.motif_asset_ready, a.primitive_wave_stamps, artifact_ok);
    SelectObject(hdc, old);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    free(snapshot);
    return ok;
}

int game_presentation_map_decision_probe(FILE *summary) {
    int old_count = civ_count, old_year = year, old_month = month;
    Civilization old_civ = civs[0];
    DecisionSnapshot first, second;
    SnapshotCiv row;
    unsigned int key_exp_a, key_exp_b, key_dip_a, key_dip_b, key_battle_a, key_battle_b, key_col_a, key_col_b;
    int refresh_ok, key_ok, ok;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&row, 0, sizeof(row));
    civ_count = 1;
    memset(&civs[0], 0, sizeof(civs[0]));
    civs[0].alive = 1;
    civs[0].uid = 9001;
    year = 24;
    month = 12;
    decision_snapshot_refresh_countdowns(0, &first);
    year = 25;
    month = 1;
    decision_snapshot_refresh_countdowns(0, &second);

    row.decision = first;
    row.decision.next_expansion_months = 2;
    key_exp_a = panel_view_model_cache_probe_decision_key(&row);
    row.decision.next_expansion_months = 1;
    key_exp_b = panel_view_model_cache_probe_decision_key(&row);
    row.decision = first;
    key_dip_a = panel_view_model_cache_probe_decision_key(&row);
    row.decision.next_diplomacy_months = second.next_diplomacy_months;
    key_dip_b = panel_view_model_cache_probe_decision_key(&row);
    row.decision = first;
    key_battle_a = panel_view_model_cache_probe_decision_key(&row);
    row.decision.next_battle_months = second.next_battle_months;
    key_battle_b = panel_view_model_cache_probe_decision_key(&row);
    row.decision = first;
    key_col_a = panel_view_model_cache_probe_decision_key(&row);
    row.decision.next_collapse_years = second.next_collapse_years;
    key_col_b = panel_view_model_cache_probe_decision_key(&row);

    refresh_ok = first.next_diplomacy_months != second.next_diplomacy_months &&
                 first.next_battle_months != second.next_battle_months &&
                 first.next_collapse_years != second.next_collapse_years;
    key_ok = key_exp_a != key_exp_b && key_dip_a != key_dip_b &&
             key_battle_a != key_battle_b && key_col_a != key_col_b;
    ok = refresh_ok && key_ok;
    fprintf(summary,
            "case=decision_countdown_refresh ok=%d refresh=%d key=%d expansion_key=%u/%u diplomacy=%d/%d battle=%d/%d collapse=%d/%d\n",
            ok, refresh_ok, key_ok, key_exp_a, key_exp_b,
            first.next_diplomacy_months, second.next_diplomacy_months,
            first.next_battle_months, second.next_battle_months,
            first.next_collapse_years, second.next_collapse_years);
    civs[0] = old_civ;
    civ_count = old_count;
    year = old_year;
    month = old_month;
    return ok;
}

static int chip_pair_fits(RECT rect, const char *label, const char *value) {
    return text_width(label) + 6 + text_width(value) <= (rect.right - rect.left) - 26;
}

int game_presentation_map_speed_probe(FILE *summary) {
    PanelMapBottomStatus en, en99, en999, en_render999, zh, zh99, zh999, zh_render999, paused;
    char badge[24], empty[24], combined[160];
    RECT client = {0, 0, 1280, 720};
    RECT rect, viewport;
    MapLayout layout;
    int old_collapsed = side_panel_collapsed;
    int old_side_w = side_panel_w;
    int old_zoom = map_zoom_percent;
    int old_x = map_offset_x;
    int old_y = map_offset_y;
    int artifact_ok;
    int ok = 1;
    int passive_hit;
    int passive_scope_blocked;
    int union_filter;
    unsigned int fail_mask = 0;

    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;

    panel_map_bottom_status_build(&en, client, 0, UI_LANG_EN, 67, 0, 1, 0);
    panel_map_bottom_status_build(&en99, client, 0, UI_LANG_EN, 67, 99, 1, 0);
    panel_map_bottom_status_build(&en999, client, 0, UI_LANG_EN, 67, 999, 1, 0);
    panel_map_bottom_status_build(&en_render999, client, 0, UI_LANG_EN, 999, 0, 1, 0);
    panel_map_bottom_status_build(&zh, client, 0, UI_LANG_ZH, 67, 0, 1, 0);
    panel_map_bottom_status_build(&zh99, client, 0, UI_LANG_ZH, 67, 99, 1, 0);
    panel_map_bottom_status_build(&zh999, client, 0, UI_LANG_ZH, 67, 999, 1, 0);
    panel_map_bottom_status_build(&zh_render999, client, 0, UI_LANG_ZH, 999, 0, 1, 0);
    panel_map_bottom_status_build(&paused, client, 0, UI_LANG_EN, 22, 3, 0, 0);
    panel_map_format_actual_speed_badge(badge, sizeof(badge), 40);
    panel_map_format_actual_speed_badge(empty, sizeof(empty), 0);
    rect = panel_map_actual_speed_badge_rect(client);
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);

    snprintf(combined, sizeof(combined), "%s %s | %s %s | %s",
             en.render_label, en.render_value, en.queue_label, en.queue_value,
             en.status_text);
    if (!(strcmp(en.render_label, "Render") == 0 && strcmp(en.render_value, "67ms") == 0)) fail_mask |= 1u << 0;
    if (!(strcmp(en.queue_label, "Queue") == 0 && strcmp(en.queue_value, "0") == 0)) fail_mask |= 1u << 1;
    if (!(strcmp(en.status_text, "Stable") == 0 && strcmp(paused.status_text, "Paused") == 0)) fail_mask |= 1u << 2;
    if (!(strcmp(zh.render_label, "渲染") == 0 && strcmp(zh.render_value, "67ms") == 0)) fail_mask |= 1u << 3;
    if (!(strcmp(zh.queue_label, "队列") == 0 && strcmp(zh.queue_value, "0") == 0)) fail_mask |= 1u << 4;
    if (strcmp(zh.status_text, "稳定") != 0) fail_mask |= 1u << 5;
    if (!(strstr(combined, "Target") == NULL && strstr(combined, "Actual") == NULL)) fail_mask |= 1u << 6;
    if (!(strstr(combined, "40ms") == NULL && strcmp(badge, "40ms") == 0)) fail_mask |= 1u << 7;
    if (!(strcmp(empty, "--ms") == 0 && panel_map_actual_speed_badge_inside_frame(client))) fail_mask |= 1u << 8;
    if (!(rect.left == viewport.left + 12 && rect.top == viewport.top + 10)) fail_mask |= 1u << 9;
    if (!(layout.map_x > viewport.left + 40 && rect.left < layout.map_x)) fail_mask |= 1u << 10;
    if (!(en.queue_rect.left > en.render_rect.right && en.status_rect.left > en.queue_rect.right)) fail_mask |= 1u << 11;
    if (text_width("Overloaded") > (en.status_rect.right - en.status_rect.left - 46)) fail_mask |= 1u << 12;
    if (!(chip_pair_fits(en.queue_rect, en.queue_label, en.queue_value) &&
          chip_pair_fits(en99.queue_rect, en99.queue_label, en99.queue_value) &&
          chip_pair_fits(en999.queue_rect, en999.queue_label, en999.queue_value))) fail_mask |= 1u << 13;
    if (!(chip_pair_fits(en.render_rect, en.render_label, en.render_value) &&
          chip_pair_fits(en_render999.render_rect, en_render999.render_label, en_render999.render_value))) fail_mask |= 1u << 14;
    if (!(chip_pair_fits(zh.queue_rect, zh.queue_label, zh.queue_value) &&
          chip_pair_fits(zh99.queue_rect, zh99.queue_label, zh99.queue_value) &&
          chip_pair_fits(zh999.queue_rect, zh999.queue_label, zh999.queue_value))) fail_mask |= 1u << 15;
    if (!(chip_pair_fits(zh.render_rect, zh.render_label, zh.render_value) &&
          chip_pair_fits(zh_render999.render_rect, zh_render999.render_label, zh_render999.render_value))) fail_mask |= 1u << 16;
    if (!(strcmp(en99.queue_value, "99") == 0 && strcmp(en999.queue_value, "999") == 0)) fail_mask |= 1u << 17;
    if (!(strcmp(zh99.queue_value, "99") == 0 && strcmp(zh999.queue_value, "999") == 0)) fail_mask |= 1u << 18;
    if (!(strcmp(en_render999.render_value, "999ms") == 0 &&
          strcmp(zh_render999.render_value, "999ms") == 0)) fail_mask |= 1u << 19;
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    diplomacy_score_tooltip_register_bar((RECT){10, 10, 90, 18}, 0, 1);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    passive_hit = ui_alliance_panel_passive_tooltip_hit(20, 14);
    if (!passive_hit) fail_mask |= 1u << 21;
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY);
    diplomacy_score_tooltip_register_bar((RECT){10, 10, 90, 18}, 0, 1);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY);
    passive_scope_blocked = !ui_alliance_panel_passive_tooltip_hit(20, 14);
    if (!passive_scope_blocked) fail_mask |= 1u << 22;
    union_filter = debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                               DEBUG_EVENT_FILTER_WAR_DIPLOMACY);
    if (!union_filter ||
        !debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                     DEBUG_EVENT_FILTER_ALL) ||
        debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                    DEBUG_EVENT_FILTER_COLLAPSE_PLAGUE)) {
        fail_mask |= 1u << 23;
    }
    artifact_ok = render_speed_badge_bmp(PRESENTATION_PROBE_DIR "/map_speed_badge_bottom_status_en.bmp",
                                         UI_LANG_EN, 0, 999, 999);
    artifact_ok &= render_speed_badge_bmp(PRESENTATION_PROBE_DIR "/map_speed_badge_bottom_status_zh.bmp",
                                          UI_LANG_ZH, 1, 999, 999);
    if (!artifact_ok) fail_mask |= 1u << 20;
    ok = fail_mask == 0;

    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side_w;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    fprintf(summary,
            "case=map_speed_status ok=%d fail_mask=0x%x render=%s/%s queue=%s/%s q99=%s q999=%s status=%s/%s badge=%s empty=%s chips=%ld,%ld,%ld rect=%ld,%ld,%ld,%ld viewport=%ld,%ld map_x=%d passive_bar=%d union_filter=%d artifact=%d files=map_speed_badge_bottom_status_en.bmp/map_speed_badge_bottom_status_zh.bmp\n",
            ok, fail_mask, en.render_label, en.render_value, en.queue_label, en.queue_value,
            en99.queue_value, en999.queue_value, zh.status_text, paused.status_text, badge, empty,
            en.render_rect.left, en.queue_rect.left, en.status_rect.left,
            rect.left, rect.top, rect.right, rect.bottom, viewport.left, viewport.top,
            layout.map_x, passive_hit && passive_scope_blocked, union_filter, artifact_ok);
    return ok;
}

int game_presentation_map_ocean_probe(FILE *summary) {
    return case_ocean_decoration_layer(summary);
}
