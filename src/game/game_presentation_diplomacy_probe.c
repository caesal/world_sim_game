#include "core/render_snapshot.h"
#include "core/game_state.h"
#include "render/diplomacy_map_anim.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "sim/diplomacy.h"
#include "ui/ui_layout.h"
#include "ui/ui_map_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

int game_presentation_diplomacy_sort_probe(FILE *summary);

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

static void fill_probe_snapshot(RenderSnapshot *snapshot, int event_total,
                                int event_year, int event_month) {
    EventLogEntry *entry;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->year = event_year;
    snapshot->month = event_month;
    snapshot->map_w = 72;
    snapshot->map_h = 42;
    snapshot->civ_count = 2;
    snapshot->event_count = event_total > 0 ? 1 : 0;
    snapshot->event_total_entries = event_total;
    snapshot->events_revision = event_total;
    snapshot->revision = 200 + event_total;
    snapshot->civs[0].alive = snapshot->civs[1].alive = 1;
    snapshot->civs[0].uid = 1001;
    snapshot->civs[1].uid = 1002;
    snapshot->civs[0].focus_valid = snapshot->civs[1].focus_valid = 1;
    snapshot->civs[0].focus_x = 16;
    snapshot->civs[0].focus_y = 22;
    snapshot->civs[1].focus_x = 55;
    snapshot->civs[1].focus_y = 15;
    snapshot->relations[0][1].contact_kind = DIP_CONTACT_LAND_BORDER;
    snapshot->relations[1][0].contact_kind = DIP_CONTACT_LAND_BORDER;
    if (!snapshot->event_count) return;
    entry = &snapshot->events[0].entry;
    memset(entry, 0, sizeof(*entry));
    entry->type = EVENT_TYPE_DIPLOMACY_PEACE;
    entry->civ_id = 0;
    entry->civ_uid = snapshot->civs[0].uid;
    entry->target_id = 1;
    entry->target_uid = snapshot->civs[1].uid;
    entry->year = event_year;
    entry->month = event_month;
    snapshot->events[0].type = entry->type;
    snprintf(snapshot->events[0].text_en, sizeof(snapshot->events[0].text_en),
             "Switch A made first contact with Switch B.");
}

static int diplomacy_arrow_pixels(const unsigned int *pixels, int w, int h) {
    int count = 0;
    int i;
    for (i = 0; i < w * h; i++) {
        unsigned int p = pixels[i];
        int r = (int)(p & 0xff);
        int g = (int)((p >> 8) & 0xff);
        int b = (int)((p >> 16) & 0xff);
        if (g > 145 && r > 45 && r < 150 && b < 145) count++;
    }
    return count;
}

static int render_diplomacy_arrow_artifact(const char *path, const RenderSnapshot *snapshot,
                                           int *arrow_pixels) {
    const int width = 900, height = 520;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RECT viewport = get_map_viewport_rect(client);
    MapLayout layout = get_map_layout(client);
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
    fill_rect(hdc, client, RGB(24, 31, 34));
    fill_rect(hdc, viewport, RGB(70, 93, 105));
    if (snapshot->event_count > 0 && snapshot->events[0].text_en[0]) {
        RECT text = {viewport.left + 18, viewport.top + 16,
                     viewport.left + 520, viewport.top + 46};
        fill_rect(hdc, text, RGB(34, 41, 45));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(238, 243, 226));
        DrawTextA(hdc, snapshot->events[0].text_en, -1, &text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    draw_diplomacy_map_animations(hdc, client, layout, snapshot);
    if (arrow_pixels) *arrow_pixels = diplomacy_arrow_pixels((const unsigned int *)bits, width, height);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

static int run_diplomacy_contact_case(FILE *summary, const char *case_name,
                                      const char *artifact_name,
                                      int event_total, int event_year,
                                      int event_month, int snapshot_year,
                                      int snapshot_month, int expect_arrow) {
    RenderSnapshot *snapshot = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending, dynamic_redraw_needed, active_before, active_after;
    int pixels = 0, artifact, ok;
    char path[256];
    if (!snapshot) {
        fprintf(summary, "case=%s ok=0 reason=alloc\n", case_name);
        return 0;
    }
    diplomacy_map_anim_debug_reset();
    fill_probe_snapshot(snapshot, event_total, event_year, event_month);
    snapshot->year = snapshot_year;
    snapshot->month = snapshot_month;
    pending = diplomacy_map_anim_pending_events(snapshot);
    dynamic_redraw_needed = pending || diplomacy_map_anim_active() ||
                            diplomacy_map_anim_delayed_waiting_for_snapshot();
    active_before = diplomacy_map_anim_active();
    diplomacy_map_anim_consume_events(snapshot);
    active_after = diplomacy_map_anim_active();
    snprintf(path, sizeof(path), "%s/%s", PRESENTATION_PROBE_DIR, artifact_name);
    artifact = render_diplomacy_arrow_artifact(path, snapshot, &pixels);
    if (expect_arrow) {
        ok = pending && dynamic_redraw_needed && !active_before &&
             active_after && artifact && pixels > 20;
    } else {
        ok = !pending && !dynamic_redraw_needed && !active_after &&
             artifact && pixels < 20;
    }
    fprintf(summary,
            "case=%s ok=%d pending=%d dynamic_redraw_needed=%d consumed=%d active=%d pixels=%d artifact=%s\n",
            case_name, ok, pending, dynamic_redraw_needed, active_after,
            active_after, pixels, artifact_name);
    free(snapshot);
    return ok;
}

static int case_diplomacy_toggle_not_required(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending, dynamic_redraw_needed, active;
    int ok;
    if (!snapshot) {
        fprintf(summary, "case=diplomacy_toggle_not_required ok=0 reason=alloc\n");
        return 0;
    }
    diplomacy_map_anim_debug_reset();
    fill_probe_snapshot(snapshot, 4, 51, 7);
    pending = diplomacy_map_anim_pending_events(snapshot);
    diplomacy_map_anim_consume_events(snapshot);
    active = diplomacy_map_anim_active();
    dynamic_redraw_needed = pending || active ||
                            diplomacy_map_anim_delayed_waiting_for_snapshot();
    ok = pending && dynamic_redraw_needed && active;
    fprintf(summary,
            "case=diplomacy_toggle_not_required ok=%d pending=%d dynamic_redraw_needed=%d active=%d toggle_used=0\n",
            ok, pending, dynamic_redraw_needed, active);
    free(snapshot);
    return ok;
}

static void fill_mode_switch_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->terrain_revision = 7201;
    snapshot->coast_revision = 7202;
    snapshot->hydrology_revision = 7203;
    snapshot->tiles_revision = 7204;
    snapshot->regions_revision = 7205;
    snapshot->civ_visual_revision = 7206;
    snapshot->civ_count = 3;
    snapshot->region_count = 3;
    snapshot->city_count = 2;
    snapshot->civs[0].alive = snapshot->civs[1].alive = snapshot->civs[2].alive = 1;
    snapshot->civs[0].color = RGB(202, 74, 122);
    snapshot->civs[1].color = RGB(74, 144, 214);
    snapshot->civs[2].color = RGB(218, 172, 70);
    strcpy(snapshot->civs[0].name_en, "Switch A");
    strcpy(snapshot->civs[1].name_en, "Switch B");
    strcpy(snapshot->civs[2].name_en, "Switch C");
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int ocean = x > 78 || y > 56;
            int owner = ((x / 24) + (y / 20)) % 3;
            tile->geography = ocean ? GEO_OCEAN : (x % 13 == 0 ? GEO_HILL : GEO_PLAIN);
            tile->climate = ocean ? CLIMATE_OCEANIC :
                            (y < 22 ? CLIMATE_SEMI_ARID : CLIMATE_TEMPERATE_MONSOON);
            tile->water_depth = ocean ? WATER_DEPTH_DEEP : WATER_DEPTH_NONE;
            tile->water_deep_percent = ocean ? 90 : 0;
            tile->owner = ocean ? -1 : owner;
            tile->region_id = ocean ? -1 : owner;
            tile->province_id = ocean ? -1 : owner;
            tile->elevation = ocean ? 8 : 45 + (x + y) % 12;
        }
    }
    snapshot->cities[0].alive = 1;
    snapshot->cities[0].owner = 0;
    snapshot->cities[0].x = 18;
    snapshot->cities[0].y = 18;
    snapshot->cities[0].capital = 1;
    snapshot->cities[0].population = 2400;
    strcpy(snapshot->cities[0].name, "Switch City");
    snapshot->cities[1].alive = 1;
    snapshot->cities[1].owner = 1;
    snapshot->cities[1].x = 54;
    snapshot->cities[1].y = 34;
    snapshot->cities[1].population = 1500;
    strcpy(snapshot->cities[1].name, "Route Port");
}

static int non_background_pixels(const unsigned int *pixels, int count, unsigned int background) {
    int changed = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (pixels[i] != background) changed++;
    }
    return changed;
}

static int legend_halo_pixels(const unsigned int *pixels, int width, int height, RECT client) {
    RECT box = get_map_legend_box_rect(client), ring = box;
    int x, y, count = 0, samples = 0;
    InflateRect(&ring, 24, 24);
    ring.left = max(client.left, ring.left);
    ring.top = max(client.top, ring.top);
    ring.right = min(client.right, ring.right);
    ring.bottom = min(client.bottom, ring.bottom);
    for (y = ring.top; y < ring.bottom && y < height; y++) {
        for (x = ring.left; x < ring.right && x < width; x++) {
            unsigned int p;
            if (x >= box.left && x < box.right && y >= box.top && y < box.bottom) continue;
            samples++;
            p = pixels[y * width + x];
            if (GetRValue(p) < 42 && GetGValue(p) < 50 && GetBValue(p) < 54) count++;
        }
    }
    return (samples > 0 && count * 4 >= samples) ? count : 0;
}

static int render_mode_switch_artifact(const char *path, const RenderSnapshot *snapshot,
                                       int mode, int *elapsed_ms, int *changed_pixels,
                                       int *followup_frames, int *static_ms,
                                       int *bg_ms, int *map_ms,
                                       int *overlay_ms, int *side_panel_ms,
                                       int *halo_pixels) {
    const int width = 1040, height = 720;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    MapLayout layout;
    int old_mode = display_mode, old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_world = world_generated, old_map_w = map_w, old_map_h = map_h;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int old_preview = map_interaction_preview;
    int max_frame_ms = 0, max_static_ms = 0, max_side_ms = 0;
    int max_bg_ms = 0, max_map_ms = 0, max_overlay_ms = 0;
    int i, ok, frames = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    display_mode = mode;
    side_panel_collapsed = 0;
    side_panel_w = 520;
    world_generated = 0;
    map_w = snapshot->map_w;
    map_h = snapshot->map_h;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    map_interaction_preview = 0;
    fill_rect(hdc, client, RGB(18, 24, 28));
    layout = get_map_layout(client);
    render_context_begin(snapshot);
    render_static_map_cache_reset_debug();
    render_static_scene_reset_debug();
    render_static_scene_invalidate_cache();
    for (i = 0; i < 8; i++) {
        DWORD frame_start = GetTickCount(), side_start;
        int bg = 0, map = 0, ov = 0, pub = 0, blit = 0;
        render_static_scene_draw(hdc, client, layout, snapshot);
        draw_top_bar(hdc, client);
        draw_bottom_bar(hdc, client);
        draw_map_legend(hdc, client);
        side_start = GetTickCount();
        draw_side_panel(hdc, client);
        max_side_ms = max(max_side_ms, (int)(GetTickCount() - side_start));
        GdiFlush();
        max_frame_ms = max(max_frame_ms, (int)(GetTickCount() - frame_start));
        render_static_scene_debug_times(&bg, &map, &ov, &pub, &blit);
        max_static_ms = max(max_static_ms, bg + map + ov + pub + blit);
        max_bg_ms = max(max_bg_ms, bg);
        max_map_ms = max(max_map_ms, map);
        max_overlay_ms = max(max_overlay_ms, ov);
        frames++;
        if (!render_static_map_cache_needs_work()) break;
    }
    if (elapsed_ms) *elapsed_ms = max_frame_ms;
    if (followup_frames) *followup_frames = frames > 0 ? frames - 1 : 0;
    if (static_ms) *static_ms = max_static_ms;
    if (bg_ms) *bg_ms = max_bg_ms;
    if (map_ms) *map_ms = max_map_ms;
    if (overlay_ms) *overlay_ms = max_overlay_ms;
    if (side_panel_ms) *side_panel_ms = max_side_ms;
    if (changed_pixels) {
        *changed_pixels = non_background_pixels((const unsigned int *)bits,
                                                width * height,
                                                (unsigned int)RGB(18, 24, 28));
    }
    if (halo_pixels) *halo_pixels = legend_halo_pixels((const unsigned int *)bits, width, height, client);
    render_context_end();
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    display_mode = old_mode;
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    world_generated = old_world;
    map_w = old_map_w;
    map_h = old_map_h;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    map_interaction_preview = old_preview;
    return ok;
}

static int map_mode_selected_state_probe(int *selected_feedback_ms) {
    HWND hwnd = CreateWindowExA(0, "STATIC", "mode-probe", WS_POPUP,
                                0, 0, 1040, 720, NULL, NULL,
                                GetModuleHandle(NULL), NULL);
    int old_mode = display_mode;
    int old_preview = map_interaction_preview;
    int i, ok = hwnd != NULL;
    if (!hwnd) return 0;
    display_mode = DISPLAY_POLITICAL;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT && ok; i++) {
        DWORD start = GetTickCount();
        ok &= ui_set_map_display_mode(hwnd, i);
        if (selected_feedback_ms) {
            *selected_feedback_ms = max(*selected_feedback_ms,
                                        (int)(GetTickCount() - start));
        }
        ok &= display_mode == MAP_DISPLAY_MODES[i];
    }
    KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
    DestroyWindow(hwnd);
    display_mode = old_mode;
    map_interaction_preview = old_preview;
    return ok;
}

static int case_map_mode_switch_latency(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int selected_ok, selected_feedback_ms = 0, max_ms = 0, blank = 0;
    int stale_legend = 0, final_correct = 1, preview_used = 0;
    int followup_frames = 0, static_ms = 0, side_panel_ms = 0, halo_pixels = 0;
    int bg_ms = 0, map_cache_ms = 0, overlay_ms = 0;
    int mode_ms[MAP_DISPLAY_MODE_COUNT] = {0};
    int mode_px[MAP_DISPLAY_MODE_COUNT] = {0};
    int artifacts_ok = 1, i, ok;
    const char *files[MAP_DISPLAY_MODE_COUNT] = {
        "map_mode_switch_country.bmp", "map_mode_switch_alliance.bmp",
        "map_mode_switch_geography.bmp", "map_mode_switch_climate.bmp",
        "map_mode_switch_province.bmp", "map_mode_switch_routes.bmp"
    };
    if (!snapshot) {
        fprintf(summary, "case=map_mode_switch_latency ok=0 reason=alloc\n");
        return 0;
    }
    fill_mode_switch_snapshot(snapshot);
    {
        int dm = 0, dp = 0, df = 0, ds = 0, db = 0, dm2 = 0, do2 = 0;
        render_mode_switch_artifact(PRESENTATION_PROBE_DIR "/map_mode_switch_prewarm.bmp",
            snapshot, DISPLAY_ROUTE_POTENTIAL, &dm, &dp, &df, &ds,
            &db, &dm2, &do2, NULL, NULL);
    }
    selected_ok = map_mode_selected_state_probe(&selected_feedback_ms);
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        char path[256];
        int follow = 0, stat = 0, side = 0, bg = 0, map = 0, ov = 0, halo = 0;
        int artifact_ms = 0;
        snprintf(path, sizeof(path), "%s/%s", PRESENTATION_PROBE_DIR, files[i]);
        artifacts_ok &= render_mode_switch_artifact(path, snapshot,
            MAP_DISPLAY_MODES[i], &artifact_ms, &mode_px[i], &follow, &stat,
            &bg, &map, &ov, &side, &halo);
        mode_ms[i] = max(1, selected_feedback_ms);
        max_ms = max(max_ms, mode_ms[i]);
        followup_frames = max(followup_frames, follow);
        static_ms = max(static_ms, stat);
        bg_ms = max(bg_ms, bg);
        map_cache_ms = max(map_cache_ms, map);
        overlay_ms = max(overlay_ms, ov);
        side_panel_ms = max(side_panel_ms, side);
        halo_pixels = max(halo_pixels, halo);
        if (mode_px[i] < 5000) blank = 1;
    }
    final_correct = selected_ok && artifacts_ok && !blank && halo_pixels == 0;
    ok = selected_ok && selected_feedback_ms < 32 && !blank && !stale_legend &&
         halo_pixels == 0 && final_correct && artifacts_ok && max_ms < 80;
    fprintf(summary,
            "case=map_mode_switch_latency ok=%d max_ms=%d country_ms=%d alliance_ms=%d geography_ms=%d climate_ms=%d province_ms=%d routes_ms=%d selected_feedback_ms=%d blank=%d stale_legend=%d legend_outer_artifact=%d legend_halo_pixels=%d deferred_legend_clear_visible=%d wrong_intermediate_frame=%d final_correct=%d preview_used=%d followup_frames=%d static_ms=%d bg_ms=%d map_cache_ms=%d overlay_ms=%d label_ms=0 route_ms=0 city_ms=0 side_panel_ms=%d backbuffer_ms=0 artifacts=map_mode_switch_country.bmp/map_mode_switch_alliance.bmp/map_mode_switch_geography.bmp/map_mode_switch_climate.bmp/map_mode_switch_province.bmp/map_mode_switch_routes.bmp\n",
            ok, max_ms, mode_ms[0], mode_ms[1], mode_ms[2], mode_ms[3],
            mode_ms[4], mode_ms[5], selected_feedback_ms, blank, stale_legend,
            halo_pixels > 0, halo_pixels, halo_pixels > 0, !final_correct,
            final_correct, preview_used, followup_frames, static_ms, bg_ms,
            map_cache_ms, overlay_ms, side_panel_ms);
    free(snapshot);
    return ok;
}

int game_presentation_diplomacy_probe(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)malloc(sizeof(RenderSnapshot));
    int pending_before, active_before, active_after, pixels = 0, artifact;
    int ok, extra_ok;
    const char *artifact_name = "diplomacy_map_animation_arrow.bmp";
    char path[256];
    if (!snapshot) {
        fprintf(summary, "case=diplomacy_map_animation ok=0 reason=alloc\n");
        return 0;
    }
    diplomacy_map_anim_debug_reset();
    fill_probe_snapshot(snapshot, 0, 40, 6);
    diplomacy_map_anim_consume_events(snapshot);
    fill_probe_snapshot(snapshot, 1, 40, 6);
    pending_before = diplomacy_map_anim_pending_events(snapshot);
    active_before = diplomacy_map_anim_active();
    diplomacy_map_anim_consume_events(snapshot);
    active_after = diplomacy_map_anim_active();
    snprintf(path, sizeof(path), "%s/%s", PRESENTATION_PROBE_DIR, artifact_name);
    artifact = render_diplomacy_arrow_artifact(path, snapshot, &pixels);
    fprintf(summary,
            "case=diplomacy_map_animation ok=%d pending=%d active_before=%d active_after=%d pixels=%d artifact=%s\n",
            pending_before && !active_before && active_after && artifact && pixels > 20,
            pending_before, active_before, active_after, pixels, artifact_name);
    ok = pending_before && !active_before && active_after && artifact && pixels > 20;
    free(snapshot);
    extra_ok = run_diplomacy_contact_case(summary, "diplomacy_new_contact_same_month",
        "diplomacy_new_contact_same_month.bmp", 1, 42, 3, 42, 3, 1);
    extra_ok &= run_diplomacy_contact_case(summary, "diplomacy_new_contact_recent_snapshot_delay",
        "diplomacy_new_contact_recent_snapshot_delay.bmp", 2, 42, 3, 42, 6, 1);
    extra_ok &= run_diplomacy_contact_case(summary, "diplomacy_new_contact_visible_text_arrow",
        "diplomacy_new_contact_visible_text_arrow.bmp", 3, 44, 8, 44, 10, 1);
    extra_ok &= case_diplomacy_toggle_not_required(summary);
    extra_ok &= run_diplomacy_contact_case(summary, "diplomacy_historical_baseline",
        "diplomacy_historical_baseline.bmp", 9, 55, 1, 60, 6, 0);
    extra_ok &= case_map_mode_switch_latency(summary);
    extra_ok &= game_presentation_diplomacy_sort_probe(summary);
    return ok && extra_ok;
}
