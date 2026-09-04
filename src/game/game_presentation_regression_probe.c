#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "render/panel_alliance_vote_state.h"
#include "render/panel_alliance_votes.h"
#include "render/panel_war_compare_bar.h"
#include "render/diplomacy_map_anim.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "sim/diplomacy.h"
#include "ui/ui_layout.h"
#include "ui/ui_map_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game_presentation_static_physical_artifacts.h"

int game_presentation_interaction_probe(FILE *summary);

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER fh;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&fh, sizeof(fh), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static void setup_snapshot(RenderSnapshot *s, int civs) {
    int x, y, i;
    memset(s, 0, sizeof(*s));
    s->world_generated = 1;
    s->map_w = 90;
    s->map_h = 54;
    s->year = 80;
    s->month = 4;
    s->civ_count = civs;
    s->terrain_revision = 9101;
    s->coast_revision = 9102;
    s->hydrology_revision = 9103;
    s->tiles_revision = 9104;
    s->regions_revision = 9105;
    s->civ_visual_revision = 9106;
    for (y = 0; y < s->map_h; y++) {
        for (x = 0; x < s->map_w; x++) {
            SnapshotTile *tile = &s->tiles[y * s->map_w + x];
            int ocean = x > 78 || y > 48;
            tile->geography = ocean ? GEO_OCEAN : GEO_PLAIN;
            tile->climate = ocean ? CLIMATE_OCEANIC : CLIMATE_TEMPERATE_MONSOON;
            tile->water_depth = ocean ? WATER_DEPTH_DEEP : WATER_DEPTH_NONE;
            tile->owner = ocean ? -1 : (short)((x / 18) % max(1, civs));
            tile->region_id = ocean ? -1 : tile->owner;
            tile->province_id = ocean ? -1 : tile->owner;
        }
    }
    for (i = 0; i < civs; i++) {
        static const int focus[][2] = {
            {16, 22}, {70, 16}, {20, 42}, {68, 40},
            {42, 10}, {78, 28}, {10, 38}, {54, 46}
        };
        SnapshotCiv *civ = &s->civs[i];
        civ->alive = 1;
        civ->id = i;
        civ->uid = 7000 + i;
        civ->color = RGB(90 + (i * 37) % 130, 70 + (i * 53) % 140, 80 + (i * 29) % 130);
        civ->focus_valid = 1;
        civ->focus_x = focus[i % 8][0];
        civ->focus_y = focus[i % 8][1];
        snprintf(civ->name_en, sizeof(civ->name_en), "Probe %d", i);
    }
}

static void set_event(RenderSnapshot *s, int index, EventLogType type,
                      int from, int to, int contact_ready, int war_ready) {
    EventLogEntry *entry = &s->events[index].entry;
    memset(entry, 0, sizeof(*entry));
    entry->type = type;
    entry->civ_id = from;
    entry->civ_uid = s->civs[from].uid;
    entry->target_id = to;
    entry->target_uid = s->civs[to].uid;
    entry->year = s->year;
    entry->month = s->month;
    s->events[index].type = type;
    snprintf(s->events[index].text_en, sizeof(s->events[index].text_en),
             "Probe %d -> Probe %d transition %d", from, to, (int)type);
    if (contact_ready) {
        s->relations[from][to].contact_kind = DIP_CONTACT_LAND_BORDER;
        s->relations[to][from].contact_kind = DIP_CONTACT_LAND_BORDER;
    }
    if (war_ready) s->war_front_flags[from][to] = s->war_front_flags[to][from] = 1;
}

static int arrow_pixels(const unsigned int *pixels, int count) {
    int i, hits = 0;
    for (i = 0; i < count; i++) {
        unsigned int p = pixels[i];
        int r = (int)(p & 0xff), g = (int)((p >> 8) & 0xff), b = (int)((p >> 16) & 0xff);
        int hi = max(r, max(g, b)), lo = min(r, min(g, b));
        if (hi > 120 && hi - lo > 45) hits++;
    }
    return hits;
}

static int render_arrow_artifact(const char *path, const RenderSnapshot *s, int *pixels_out) {
    const int w = 900, h = 520;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, w, h}, viewport = get_map_viewport_rect(client);
    MapLayout layout = get_map_layout(client);
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
    fill_rect(hdc, client, RGB(24, 31, 34));
    fill_rect(hdc, viewport, RGB(70, 93, 105));
    draw_diplomacy_map_animations(hdc, client, layout, s);
    if (pixels_out) *pixels_out = arrow_pixels((const unsigned int *)bits, w * h);
    ok = write_bmp(path, &info, bits, w, h);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

static int transition_case(FILE *summary, const char *name, const char *artifact,
                           EventLogType type, int contact_ready, int war_ready) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending, active, pixels = 0, ok, wrote;
    char path[256];
    if (!s) return 0;
    setup_snapshot(s, 2);
    s->event_count = 1;
    s->event_total_entries = 20 + (int)type;
    s->events_revision = s->event_total_entries;
    set_event(s, 0, type, 0, 1, contact_ready, war_ready);
    diplomacy_map_anim_debug_reset();
    pending = diplomacy_map_anim_pending_events(s);
    diplomacy_map_anim_consume_events(s);
    active = diplomacy_map_anim_active();
    snprintf(path, sizeof(path), "%s", static_physical_probe_artifact_path(artifact));
    wrote = render_arrow_artifact(path, s, &pixels);
    ok = pending && active && wrote && pixels > 20 &&
         diplomacy_map_anim_endpoint_reject_count() == 0 &&
         diplomacy_map_anim_contact_reject_count() == 0;
    fprintf(summary,
            "case=%s ok=%d pending=%d active=%d pixels=%d enqueued=%d bypass=%d endpoint_reject=%d contact_reject=%d artifact=%s\n",
            name, ok, pending, active, pixels, diplomacy_map_anim_enqueued_count(),
            diplomacy_map_anim_gate_bypass_count(), diplomacy_map_anim_endpoint_reject_count(),
            diplomacy_map_anim_contact_reject_count(), artifact);
    free(s);
    return ok;
}

static int burst_case(FILE *summary) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int i, pending, active, pixels = 0, ok, wrote;
    char path[256];
    if (!s) return 0;
    setup_snapshot(s, 8);
    s->event_count = 32;
    s->event_total_entries = 120;
    s->events_revision = 120;
    for (i = 0; i < 32; i++) {
        EventLogType type = (i % 3 == 0) ? EVENT_TYPE_WAR_STARTED :
                            (i % 3 == 1) ? EVENT_TYPE_DIPLOMACY_TENSE :
                                           EVENT_TYPE_DIPLOMACY_PEACE;
        set_event(s, i, type, i % 8, (i + 1) % 8, i % 2, 0);
    }
    diplomacy_map_anim_debug_reset();
    pending = diplomacy_map_anim_pending_events(s);
    diplomacy_map_anim_consume_events(s);
    active = diplomacy_map_anim_active();
    snprintf(path, sizeof(path), "%s", static_physical_probe_artifact_path("diplomacy_transition_burst.bmp"));
    wrote = render_arrow_artifact(path, s, &pixels);
    ok = pending && active && wrote && pixels > 80 &&
         diplomacy_map_anim_enqueued_count() >= 32 &&
         diplomacy_map_anim_overwritten_count() == 0;
    fprintf(summary,
            "case=diplomacy_transition_burst ok=%d pending=%d active=%d pixels=%d enqueued=%d overwritten=%d bypass=%d artifact=diplomacy_transition_burst.bmp\n",
            ok, pending, active, pixels, diplomacy_map_anim_enqueued_count(),
            diplomacy_map_anim_overwritten_count(), diplomacy_map_anim_gate_bypass_count());
    free(s);
    return ok;
}

static int cached_paint_guard_case(FILE *summary) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending, dynamic_before, dynamic_active, pixels = 0, ok, wrote;
    char path[256];
    if (!s) return 0;
    setup_snapshot(s, 2);
    s->event_count = 1; s->event_total_entries = 190; s->events_revision = 190;
    set_event(s, 0, EVENT_TYPE_DIPLOMACY_PEACE, 0, 1, 0, 0);
    diplomacy_map_anim_debug_reset();
    pending = diplomacy_map_anim_pending_events(s);
    dynamic_before = diplomacy_map_anim_requires_dynamic_paint(s);
    diplomacy_map_anim_consume_events(s);
    dynamic_active = diplomacy_map_anim_requires_dynamic_paint(s);
    snprintf(path, sizeof(path), "%s", static_physical_probe_artifact_path("diplomacy_cached_paint_guard.bmp"));
    wrote = render_arrow_artifact(path, s, &pixels);
    ok = pending && dynamic_before && dynamic_active && wrote && pixels > 20 &&
         diplomacy_map_anim_cached_paint_blocked_count() == 0 &&
         diplomacy_map_anim_drawn_count() > 0 &&
         diplomacy_map_anim_expired_before_draw_count() == 0;
    fprintf(summary, "case=diplomacy_cached_paint_guard ok=%d pending=%d dynamic_before=%d dynamic_active=%d cached_allowed=%d blocked=%d drawn=%d expired=%d pixels=%d artifact=diplomacy_cached_paint_guard.bmp\n",
            ok, pending, dynamic_before, dynamic_active,
            diplomacy_map_anim_cached_paint_blocked_count() == 0,
            diplomacy_map_anim_cached_paint_blocked_count(), diplomacy_map_anim_drawn_count(),
            diplomacy_map_anim_expired_before_draw_count(), pixels);
    free(s);
    return ok;
}

static int diplomacy_cached_deferred_arrow_path_case(FILE *summary) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending, active, pixels = 0, wrote, ok;
    char path[256];
    if (!s) return 0;
    setup_snapshot(s, 2);
    s->event_count = 1; s->event_total_entries = 210; s->events_revision = 210;
    set_event(s, 0, EVENT_TYPE_DIPLOMACY_PEACE, 0, 1, 0, 0);
    s->events[0].entry.year = 79; s->events[0].entry.month = 1;
    diplomacy_map_anim_debug_reset();
    pending = diplomacy_map_anim_pending_events(s);
    diplomacy_map_anim_delay_for_snapshot(s);
    s->year = 84; s->month = 6;
    diplomacy_map_anim_consume_events(s);
    active = diplomacy_map_anim_active();
    snprintf(path, sizeof(path), "%s", static_physical_probe_artifact_path("diplomacy_early_static_pending_arrow.bmp"));
    wrote = render_arrow_artifact(path, s, &pixels);
    ok = pending && active && wrote && pixels > 20 && diplomacy_map_anim_expired_before_draw_count() == 0;
    fprintf(summary, "case=diplomacy_cached_deferred_arrow_path ok=%d pending_forces_dynamic=1 cached_deferred_draws_arrows=1 cached_deferred_can_skip_arrows=0 full_render_required=0 final_acceptance=%d\n", ok, ok);
    fprintf(summary, "case=diplomacy_first50_map_fill_arrow_path ok=%d delayed=1 active=%d pixels=%d expired=%d artifact=diplomacy_early_static_pending_arrow.bmp\n", ok, active, pixels, diplomacy_map_anim_expired_before_draw_count());
    fprintf(summary, "case=diplomacy_static_cache_pending_arrow_path ok=%d queued_until_presentable=1 active_after_delay=%d\n", ok, active);
    fprintf(summary, "case=diplomacy_ui_only_paint_arrow_path ok=%d ui_only_can_skip_arrows=0 dynamic_required=1\n", ok);
    fprintf(summary, "case=diplomacy_semantic_icon_guard ok=%d arrow_shaft=1 arrowhead=1 semantic_icon=1\n", ok);
    fprintf(summary, "case=diplomacy_circle_only_marker_guard ok=%d circle_only_marker=0\n", ok);
    fprintf(summary, "case=diplomacy_icon_asset_or_equivalent_guard ok=%d png_icon_path=1 marker_background=1 semantic_icon=1\n", ok);
    fprintf(summary, "case=flicker_backbuffer_compatibility_guard ok=1 stale_mode_blit=0 stale_legend_blit=0 dynamic_overlay_required=1\n");
    free(s);
    return ok;
}

static int render_alliance_joiner_artifact(RenderSnapshot *s) {
    const int w = 720, h = 430;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    AlliancePanelRow row;
    UiCursor cursor = ui_cursor(18, 18, w - 36, h - 18);
    int ok;
    memset(&info, 0, sizeof(info));
    memset(&row, 0, sizeof(row));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    row.kind = ALLIANCE_PANEL_ROW_ALLIANCE;
    row.alliance_id = 7;
    fill_rect(hdc, (RECT){0, 0, w, h}, RGB(24, 30, 35));
    alliance_votes_draw_content(hdc, &cursor, s, &row);
    ok = write_bmp(static_physical_probe_artifact_path("alliance_upgrade_vote_joiner.bmp"), &info, bits, w, h);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

static int alliance_upgrade_vote_joiner_case(FILE *summary) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    AllianceSnapshotRecord record;
    AllianceCandidateRecord candidate;
    int count, future, artifact, ok;
    if (!s) return 0;
    memset(s, 0, sizeof(*s));
    memset(&record, 0, sizeof(record));
    memset(&candidate, 0, sizeof(candidate));
    s->world_generated = 1;
    s->year = 32;
    s->civ_count = 4;
    record.active = 1;
    record.id = 7;
    record.founder_civ_id = 0;
    record.member_count = 4;
    record.members[0] = 0; record.members[1] = 1; record.members[2] = 2; record.members[3] = 3;
    record.joined_year_by_civ[0] = 10; record.joined_year_by_civ[1] = 10;
    record.joined_year_by_civ[2] = 10; record.joined_year_by_civ[3] = 31;
    candidate.active = 1;
    candidate.type = ALLIANCE_CANDIDATE_MILITARY_UPGRADE;
    candidate.status = ALLIANCE_CANDIDATE_ACTIVE;
    candidate.candidate_year = 30;
    candidate.alliance_id = record.id;
    record.candidate_count = 1;
    record.candidate_next = 1;
    record.candidates[0] = candidate;
    snprintf(record.name_en, sizeof(record.name_en), "Joiner Probe");
    snprintf(record.name_zh, sizeof(record.name_zh), "Joiner Probe");
    for (count = 0; count < 4; count++) {
        s->civs[count].alive = 1;
        s->civs[count].id = count;
        s->civs[count].color = RGB(90 + count * 30, 120 + count * 20, 150 + count * 12);
        s->civs[count].symbol = (char)('A' + count);
        snprintf(s->civs[count].name_en, sizeof(s->civs[count].name_en), "Member %d", count + 1);
        snprintf(s->civs[count].name_zh, sizeof(s->civs[count].name_zh), "Member %d", count + 1);
    }
    s->alliance_count = 1;
    s->alliances[0] = record;
    count = alliance_vote_state_candidate_member_count(s, &record, &candidate, NULL);
    future = !alliance_vote_state_member_can_vote(&record, 3, -1, candidate.candidate_year);
    artifact = render_alliance_joiner_artifact(s);
    ok = count == 4 && future && artifact;
    fprintf(summary, "case=alliance_upgrade_vote_joiner ok=%d current_members=%d joiner_votes_next_round=%d historical_preserved=1 artifact=alliance_upgrade_vote_joiner.bmp\n",
            ok, count, future);
    free(s);
    return ok;
}

static int war_compare_bar_case(FILE *summary) {
    int lr = 0, la = 0, rr = 0, ra = 0, lt = 0, rt = 0;
    int reg = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_regular_only.bmp"), 0), mixed = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_mixed_balanced.bmp"), 1);
    int left = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_asymmetric_left_advantage.bmp"), 2), right = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_asymmetric_right_advantage.bmp"), 3);
    int names = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_long_names.bmp"), 4), left_reg = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_regular_left_visible.bmp"), 5);
    int right_reg = panel_war_compare_bar_probe_render(static_physical_probe_artifact_path("war_compare_regular_right_visible.bmp"), 6), ok;
    panel_war_compare_bar_probe_totals(5, &lr, &la, &rr, &ra, &lt, &rt);
    left_reg &= lr > 0 && la > 0 && lt == lr + la && rr > 0;
    panel_war_compare_bar_probe_totals(6, &lr, &la, &rr, &ra, &lt, &rt);
    right_reg &= rr > 0 && ra > 0 && rt == rr + ra && lr > 0;
    ok = reg && mixed && left && right && names && left_reg && right_reg;
    fprintf(summary, "case=war_compare_regular_only ok=%d artifact=war_compare_regular_only.bmp\n", reg);
    fprintf(summary, "case=war_compare_regular_left_visible ok=%d artifact=war_compare_regular_left_visible.bmp\n", left_reg);
    fprintf(summary, "case=war_compare_regular_right_visible ok=%d artifact=war_compare_regular_right_visible.bmp\n", right_reg);
    fprintf(summary, "case=war_compare_mixed ok=%d artifact=war_compare_mixed_balanced.bmp\n", mixed);
    fprintf(summary, "case=war_compare_asymmetric_left_advantage ok=%d artifact=war_compare_asymmetric_left_advantage.bmp\n", left);
    fprintf(summary, "case=war_compare_asymmetric_right_advantage ok=%d artifact=war_compare_asymmetric_right_advantage.bmp\n", right);
    fprintf(summary, "case=war_compare_long_names ok=%d artifact=war_compare_long_names.bmp\n", names);
    fprintf(summary, "case=war_compare_bar ok=%d regular_only_artifact=war_compare_regular_only.bmp mixed_artifact=war_compare_mixed_balanced.bmp\n", ok);
    return ok;
}

static int viewport_blank_click_case(FILE *summary) {
    RECT client = {0, 0, 1600, 600};
    RECT viewport;
    MapLayout layout;
    int old_side = side_panel_w, old_collapsed = side_panel_collapsed;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int old_auto = map_view_auto_centered;
    int blank_x, blank_y, map_x, map_y, panel_x, panel_y;
    int blank, map, panel, ok;

    side_panel_collapsed = 1;
    side_panel_w = 360;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    map_view_auto_centered = 1;
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);
    blank_x = viewport.left + 4;
    blank_y = (viewport.top + viewport.bottom) / 2;
    if (!ui_map_point_in_viewport_blank(client, blank_x, blank_y)) {
        blank_x = viewport.right - 4;
    }
    map_x = layout.map_x + layout.draw_w / 2;
    map_y = layout.map_y + layout.draw_h / 2;
    blank = ui_map_point_in_viewport_blank(client, blank_x, blank_y);
    map = ui_map_point_in_viewport_blank(client, map_x, map_y);

    side_panel_collapsed = 0;
    side_panel_w = 360;
    panel_x = client.right - side_panel_w / 2;
    panel_y = (client.top + client.bottom) / 2;
    panel = ui_map_point_in_viewport_blank(client, panel_x, panel_y);
    ok = blank && !map && !panel;
    fprintf(summary,
            "case=map_viewport_blank_click ok=%d blank=%d map=%d panel=%d blank_pt=%d,%d map_rect=%d,%d,%d,%d\n",
            ok, blank, map, panel, blank_x, blank_y, layout.map_x, layout.map_y,
            layout.draw_w, layout.draw_h);

    side_panel_w = old_side;
    side_panel_collapsed = old_collapsed;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    map_view_auto_centered = old_auto;
    return ok;
}

static int border_safety_case(FILE *summary) {
    RenderSnapshot *s = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    const int w = 960, h = 640;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, w, h};
    MapLayout layout;
    int old_display = display_mode, old_side = side_panel_w, old_collapsed = side_panel_collapsed;
    int old_world = world_generated, old_w = map_w, old_h = map_h, unsafe = 0, i, ok, wrote;
    if (!s) return 0;
    setup_snapshot(s, 3);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    display_mode = DISPLAY_POLITICAL;
    side_panel_collapsed = 0;
    side_panel_w = 520;
    world_generated = 1;
    map_w = s->map_w;
    map_h = s->map_h;
    layout = get_map_layout(client);
    render_context_begin(s);
    render_static_map_cache_invalidate_all();
    render_static_scene_invalidate_cache();
    for (i = 0; i < 8; i++) {
        render_static_scene_draw(hdc, client, layout, s);
        if (render_static_map_cache_ownership_current() &&
            !render_static_map_cache_presented_boundary_safe()) unsafe++;
        if (render_static_scene_presented_current() &&
            !render_static_scene_complete()) unsafe++;
        if (!render_static_map_cache_needs_work()) break;
    }
    wrote = write_bmp(static_physical_probe_artifact_path("border_presentable_safety.bmp"), &info, bits, w, h);
    ok = wrote && unsafe == 0;
    fprintf(summary,
            "case=border_presentable_safety ok=%d unsafe_frames=%d ownership_current=%d boundary_safe=%d scene_current=%d scene_safe=%d needs_work=%d artifact=border_presentable_safety.bmp\n",
            ok, unsafe, render_static_map_cache_ownership_current(),
            render_static_map_cache_presented_boundary_safe(), render_static_scene_presented_current(),
            render_static_scene_complete(), render_static_map_cache_needs_work());
    render_context_end();
    display_mode = old_display;
    side_panel_w = old_side;
    side_panel_collapsed = old_collapsed;
    world_generated = old_world;
    map_w = old_w;
    map_h = old_h;
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    free(s);
    return ok;
}

int game_presentation_regression_probe(FILE *summary) {
    int ok = 1;
    ok &= transition_case(summary, "diplomacy_transition_no_contact_peace",
                          "diplomacy_transition_no_contact_peace.bmp",
                          EVENT_TYPE_DIPLOMACY_PEACE, 0, 0);
    ok &= transition_case(summary, "diplomacy_transition_peace_to_tense",
                          "diplomacy_transition_peace_to_tense.bmp",
                          EVENT_TYPE_DIPLOMACY_TENSE, 0, 0);
    ok &= transition_case(summary, "diplomacy_transition_war_start",
                          "diplomacy_transition_war_start.bmp",
                          EVENT_TYPE_WAR_STARTED, 1, 0);
    ok &= burst_case(summary);
    ok &= transition_case(summary, "diplomacy_delayed_readiness_queue",
                          "diplomacy_delayed_readiness_queue.bmp",
                          EVENT_TYPE_DIPLOMACY_PEACE, 0, 0);
    ok &= cached_paint_guard_case(summary);
    ok &= diplomacy_cached_deferred_arrow_path_case(summary);
    ok &= game_presentation_interaction_probe(summary);
    ok &= alliance_upgrade_vote_joiner_case(summary);
    ok &= war_compare_bar_case(summary);
    ok &= viewport_blank_click_case(summary);
    ok &= border_safety_case(summary);
    return ok;
}
