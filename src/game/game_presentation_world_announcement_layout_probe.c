#include "game/game_presentation_world_announcement_probe.h"

#include "core/render_snapshot.h"
#include "core/world_announcement_store.h"
#include "render/panel_map_speed_badge.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "render/render_partial_ui.h"
#include "render/render_static_scene.h"
#include "render/render_transient_ui.h"
#include "render/top_world_announcement.h"
#include "ui/ui_layout.h"
#include "ui/ui_notifications.h"
#include "ui/ui_theme.h"
#include "ui/world_announcement_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_DIR "build/validation/presentation_probe_20260618"

typedef struct {
    HDC screen;
    HDC dc;
    HBITMAP bitmap;
    HBITMAP previous;
    BITMAPINFO info;
    unsigned char *bits;
    int width;
    int height;
} ProbeSurface;

typedef struct {
    int language;
    int side_width;
    int side_collapsed;
    int mode;
    int selected_country;
    int selected_tile_x;
    int selected_tile_y;
    int hover_client_x;
    int hover_client_y;
} ProbeUiState;

static int surface_create(ProbeSurface *surface, int width, int height) {
    memset(surface, 0, sizeof(*surface));
    surface->screen = GetDC(NULL);
    surface->dc = CreateCompatibleDC(surface->screen);
    surface->width = width;
    surface->height = height;
    surface->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    surface->info.bmiHeader.biWidth = width;
    surface->info.bmiHeader.biHeight = -height;
    surface->info.bmiHeader.biPlanes = 1;
    surface->info.bmiHeader.biBitCount = 32;
    surface->info.bmiHeader.biCompression = BI_RGB;
    surface->bitmap = CreateDIBSection(surface->screen, &surface->info,
        DIB_RGB_COLORS, (void **)&surface->bits, NULL, 0);
    if (!surface->screen || !surface->dc || !surface->bitmap || !surface->bits) return 0;
    surface->previous = SelectObject(surface->dc, surface->bitmap);
    return 1;
}

static void surface_destroy(ProbeSurface *surface) {
    if (surface->dc && surface->previous) SelectObject(surface->dc, surface->previous);
    if (surface->bitmap) DeleteObject(surface->bitmap);
    if (surface->dc) DeleteDC(surface->dc);
    if (surface->screen) ReleaseDC(NULL, surface->screen);
    memset(surface, 0, sizeof(*surface));
}

static int surface_write(const ProbeSurface *surface, const char *path) {
    BITMAPFILEHEADER header = {0};
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits +
                    (DWORD)(surface->width * surface->height * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&surface->info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(surface->bits, (size_t)(surface->width * surface->height * 4), 1, file);
    fclose(file);
    return 1;
}

static COLORREF surface_pixel(const ProbeSurface *surface, int x, int y) {
    const unsigned char *pixel = surface->bits + ((size_t)y * surface->width + x) * 4;
    return RGB(pixel[2], pixel[1], pixel[0]);
}

static void fill_pattern(ProbeSurface *surface, RECT client, int phase) {
    RECT viewport = get_map_viewport_rect(client);
    int x, y;
    fill_rect(surface->dc, client, RGB(18, 24, 28));
    for (y = viewport.top; y < viewport.bottom; y += 24) {
        for (x = viewport.left; x < viewport.right; x += 40) {
            int bit = ((x - viewport.left) / 40 + (y - viewport.top) / 24 + phase) & 1;
            COLORREF color = bit ? RGB(112 + phase * 7, 152, 118) :
                                   RGB(72, 112 + phase * 9, 158);
            fill_rect(surface->dc, (RECT){x, y, min(x + 40, viewport.right),
                                          min(y + 24, viewport.bottom)}, color);
        }
    }
}

static int rect_inside(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom &&
           inner.right > inner.left && inner.bottom > inner.top;
}

static int rect_overlap_area(RECT a, RECT b) {
    RECT overlap;
    if (!IntersectRect(&overlap, &a, &b)) return 0;
    return (overlap.right - overlap.left) * (overlap.bottom - overlap.top);
}

static ProbeUiState save_ui_state(void) {
    ProbeUiState state = {ui_language, side_panel_w, side_panel_collapsed,
        display_mode, selected_civ, selected_x, selected_y, hover_x, hover_y};
    return state;
}

static void restore_ui_state(ProbeUiState state) {
    ui_language = state.language;
    side_panel_w = state.side_width;
    side_panel_collapsed = state.side_collapsed;
    display_mode = state.mode;
    selected_civ = state.selected_country;
    selected_x = state.selected_tile_x;
    selected_y = state.selected_tile_y;
    hover_x = state.hover_client_x;
    hover_y = state.hover_client_y;
}

static void prepare_event(RenderSnapshot *snapshot,
                          const WorldAnnouncementEvent *event, unsigned int revision) {
    world_announcement_store_clear();
    world_announcement_queue_reset();
    world_announcement_store_append(event);
    snapshot->revision = revision;
    snapshot->world_generated = 1;
    snapshot->world_announcement_count = 1;
    snapshot->world_announcement_total_entries = world_announcement_store_total_entries();
    snapshot->world_announcements[0].event_id = event->event_id;
    snapshot->world_announcements[0].event_type = event->event_type;
    snapshot->world_announcements[0].priority = event->priority;
    snapshot->world_announcements[0].year = event->year;
    snapshot->world_announcements[0].month = event->month;
}

static int map_overlay_layout_case(FILE *summary) {
    const RECT clients[] = {{0, 0, 2560, 1440}, {0, 0, 1920, 1080},
                            {0, 0, 1366, 768}};
    ProbeUiState saved = save_ui_state();
    int inside = 1, top_overlap = 0, badge_overlap = 0, panel_overlap = 0;
    int bottom_overlap = 0, toast_overlap = 0, controls = 1;
    int c, collapsed, control;
    side_panel_w = 380;
    for (c = 0; c < 3; c++) for (collapsed = 0; collapsed <= 1; collapsed++) {
        RECT client = clients[c];
        RECT viewport;
        RECT band;
        RECT badge;
        RECT panel;
        RECT handle;
        RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H,
                       client.right, client.bottom};
        RECT top = {client.left, client.top, client.right, TOP_BAR_H};
        RECT toast;
        side_panel_collapsed = collapsed;
        viewport = get_map_viewport_rect(client);
        band = get_world_announcement_rect(client);
        badge = panel_map_actual_speed_badge_rect(client);
        panel = get_side_panel_draw_rect(client);
        handle = get_side_panel_handle_rect(client);
        toast = ui_notifications_rect_for_announcement(client, 1);
        inside &= rect_inside(viewport, band) && band.top == viewport.top + 8 &&
                  band.bottom - band.top == 82;
        top_overlap += rect_overlap_area(band, top);
        badge_overlap += rect_overlap_area(band, badge);
        if (!collapsed) panel_overlap += rect_overlap_area(band, panel) +
                                        rect_overlap_area(band, handle);
        bottom_overlap += rect_overlap_area(band, bottom);
        toast_overlap += rect_overlap_area(band, toast);
        controls &= toast.top == band.bottom + 8;
        for (control = WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS;
             control <= WORLD_ANNOUNCEMENT_CONTROL_DISMISS; control++) {
            controls &= rect_inside(band, get_world_announcement_control_rect(
                client, (WorldAnnouncementControl)control));
        }
    }
    restore_ui_state(saved);
    fprintf(summary,
        "case=world_announcement_map_overlay_layout ok=%d below_top=%d inside_viewport=%d top_overlap=%d badge_overlap=%d panel_overlap=%d bottom_overlap=%d toast_overlap=%d controls=%d\n",
        inside && controls && !top_overlap && !badge_overlap && !panel_overlap &&
        !bottom_overlap && !toast_overlap, inside, inside, top_overlap, badge_overlap,
        panel_overlap, bottom_overlap, toast_overlap, controls);
    return inside && controls && !top_overlap && !badge_overlap && !panel_overlap &&
           !bottom_overlap && !toast_overlap;
}

static int topbar_preserved_case(FILE *summary,
                                 const WorldAnnouncementEvent *event) {
    ProbeSurface active = {0}, inactive = {0};
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    ProbeUiState saved = save_ui_state();
    RECT client = {0, 0, 1920, TOP_BAR_H};
    int diff = -1, date_pixels = 0, x, y, wrote = 0;
    if (!snapshot || !surface_create(&active, 1920, TOP_BAR_H) ||
        !surface_create(&inactive, 1920, TOP_BAR_H)) goto done;
    side_panel_w = 380;
    side_panel_collapsed = 0;
    hover_x = hover_y = -1;
    prepare_event(snapshot, event, 10);
    world_announcement_queue_consume(snapshot);
    fill_rect(active.dc, client, RGB(7, 9, 11));
    draw_top_bar(active.dc, client);
    world_announcement_queue_reset();
    fill_rect(inactive.dc, client, RGB(7, 9, 11));
    draw_top_bar(inactive.dc, client);
    diff = memcmp(active.bits, inactive.bits, (size_t)1920 * TOP_BAR_H * 4) != 0;
    for (y = 9; y < 50; y++) for (x = 848; x < 1072; x++) {
        COLORREF pixel = surface_pixel(&active, x, y);
        if (GetRValue(pixel) > 150 && GetGValue(pixel) > 125 && GetBValue(pixel) < 190)
            date_pixels++;
    }
    wrote = surface_write(&active, PROBE_DIR "/world_announcement_topbar_active.bmp") &&
            surface_write(&inactive, PROBE_DIR "/world_announcement_topbar_inactive.bmp");
done:
    fprintf(summary,
        "case=world_announcement_topbar_preserved ok=%d pixel_diff=%d year_month_visible=%d top_height=%d artifact=%s\n",
        diff == 0 && date_pixels > 0 && wrote, diff, date_pixels > 0, TOP_BAR_H,
        "world_announcement_topbar_active.bmp");
    if (active.dc) surface_destroy(&active);
    if (inactive.dc) surface_destroy(&inactive);
    free(snapshot);
    world_announcement_queue_reset();
    world_announcement_store_clear();
    restore_ui_state(saved);
    return diff == 0 && date_pixels > 0 && wrote;
}

static int expected_channel(int foreground, int background, int alpha) {
    return (foreground * alpha + background * (255 - alpha) + 127) / 255;
}

static int alpha_typography_cases(FILE *summary,
                                  const WorldAnnouncementEvent *event) {
    ProbeSurface surface;
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    ProbeUiState saved = save_ui_state();
    RECT client = {0, 0, 1366, 220};
    RECT band;
    COLORREF before = 0, after = 0;
    TopWorldAnnouncementProbeInfo info = {0};
    int x, y, formula = 0, retained = 0, wrote = 0, typography;
    int overlay_alpha = ui_theme_overlay_alpha();
    memset(&surface, 0, sizeof(surface));
    if (!snapshot || !surface_create(&surface, client.right, client.bottom)) goto done;
    side_panel_w = 380;
    side_panel_collapsed = 0;
    display_mode = DISPLAY_POLITICAL;
    prepare_event(snapshot, event, 20);
    render_context_begin(snapshot);
    fill_pattern(&surface, client, 0);
    draw_top_bar(surface.dc, client);
    band = get_world_announcement_rect(client);
    x = band.left + 36;
    y = band.bottom - 10;
    before = surface_pixel(&surface, x, y);
    render_transient_ui_draw_full(surface.dc, client, 0);
    after = surface_pixel(&surface, x, y);
    info = top_world_announcement_probe_info();
    formula = abs(GetRValue(after) - expected_channel(
                      24, GetRValue(before), overlay_alpha)) <= 2 &&
              abs(GetGValue(after) - expected_channel(
                      30, GetGValue(before), overlay_alpha)) <= 2 &&
              abs(GetBValue(after) - expected_channel(
                      34, GetBValue(before), overlay_alpha)) <= 2;
    retained = after != before && after != RGB(24, 30, 34);
    wrote = surface_write(&surface, PROBE_DIR "/world_announcement_alpha_map_overlay.bmp");
    render_context_end();
done:
    typography = info.header_font_px >= 20 && info.body_font_px >= 18 &&
                 info.metadata_font_px >= 16 && info.minimum_body_font_px >= 18 &&
                 !info.compact_shrink_enabled;
    fprintf(summary,
        "case=world_announcement_alpha_composite ok=%d alpha=%d patterned_underlay=%d formula_tolerance=%d before=%u after=%u artifact=%s\n",
        formula && retained && overlay_alpha == 128 &&
        info.background_alpha == overlay_alpha && wrote,
        info.background_alpha, retained, formula, (unsigned int)before,
        (unsigned int)after, "world_announcement_alpha_map_overlay.bmp");
    fprintf(summary,
        "case=world_announcement_typography ok=%d header=%d body=%d metadata=%d minimum_body=%d compact_shrink=%d\n",
        typography, info.header_font_px, info.body_font_px, info.metadata_font_px,
        info.minimum_body_font_px, info.compact_shrink_enabled);
    if (surface.dc) surface_destroy(&surface);
    free(snapshot);
    world_announcement_queue_reset();
    world_announcement_store_clear();
    restore_ui_state(saved);
    return formula && retained && overlay_alpha == 128 &&
           info.background_alpha == overlay_alpha && wrote && typography;
}

static int underlay_freshness_case(FILE *summary,
                                   const WorldAnnouncementEvent *event) {
    ProbeSurface surface;
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    ProbeUiState saved = save_ui_state();
    RECT client = {0, 0, 1366, 220};
    RECT band;
    COLORREF old_pixel = 0, expected = 0, actual = 0;
    int x, y, stale_rejected = 0, restored = 0, wrote = 0;
    memset(&surface, 0, sizeof(surface));
    if (!snapshot || !surface_create(&surface, client.right, client.bottom)) goto done;
    side_panel_w = 380;
    side_panel_collapsed = 0;
    display_mode = DISPLAY_POLITICAL;
    prepare_event(snapshot, event, 30);
    render_context_begin(snapshot);
    fill_pattern(&surface, client, 0);
    draw_top_bar(surface.dc, client);
    band = get_world_announcement_rect(client);
    x = band.left + 36;
    y = band.bottom - 10;
    old_pixel = surface_pixel(&surface, x, y);
    render_transient_ui_draw_full(surface.dc, client, 0);
    display_mode = DISPLAY_ALLIANCE;
    snapshot->revision = 31;
    stale_rejected = !render_transient_ui_can_partial(client, band);
    fill_pattern(&surface, client, 1);
    draw_top_bar(surface.dc, client);
    expected = surface_pixel(&surface, x, y);
    render_transient_ui_draw_full(surface.dc, client, 0);
    world_announcement_queue_dismiss();
    render_transient_ui_reset_probe_metrics();
    render_partial_ui_draw(surface.dc, client, band);
    actual = surface_pixel(&surface, x, y);
    restored = render_transient_ui_underlay_restores() == 1 && actual == expected;
    wrote = surface_write(&surface, PROBE_DIR "/world_announcement_dismissed_fresh_underlay.bmp");
    render_context_end();
done:
    fprintf(summary,
        "case=world_announcement_underlay_freshness ok=%d mode_revision_rejected=%d latest_map=%d stale_rectangle=%d artifact=%s\n",
        stale_rejected && restored && old_pixel != expected && wrote, stale_rejected,
        restored, actual == old_pixel && old_pixel != expected,
        "world_announcement_dismissed_fresh_underlay.bmp");
    if (surface.dc) surface_destroy(&surface);
    free(snapshot);
    world_announcement_queue_reset();
    world_announcement_store_clear();
    restore_ui_state(saved);
    return stale_rejected && restored && old_pixel != expected && wrote;
}

static int partial_present_case(FILE *summary,
                                const WorldAnnouncementEvent *event) {
    ProbeSurface surface;
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    ProbeUiState saved = save_ui_state();
    RECT client = {0, 0, 1366, 220};
    RECT band, top, side, bottom;
    int before = 0, after = 0, partial = 0, restored = 0;
    int top_invalidations = 0, side_invalidations = 0, bottom_invalidations = 0;
    memset(&surface, 0, sizeof(surface));
    if (!snapshot || !surface_create(&surface, client.right, client.bottom)) goto done;
    side_panel_w = 380;
    side_panel_collapsed = 0;
    display_mode = DISPLAY_POLITICAL;
    prepare_event(snapshot, event, 40);
    render_context_begin(snapshot);
    fill_pattern(&surface, client, 0);
    draw_top_bar(surface.dc, client);
    render_transient_ui_draw_full(surface.dc, client, 0);
    band = get_world_announcement_rect(client);
    hover_x = band.right - 20;
    hover_y = band.top + 18;
    world_announcement_queue_set_hovered(1);
    world_announcement_queue_tick(GetTickCount() + 100);
    render_static_scene_reset_debug();
    render_transient_ui_reset_probe_metrics();
    before = render_scene_cache_viewport_rebuilds();
    partial = render_partial_ui_can_paint(client, band, snapshot);
    render_partial_ui_draw(surface.dc, client, band);
    after = render_scene_cache_viewport_rebuilds();
    restored = render_transient_ui_underlay_restores();
    top = (RECT){client.left, client.top, client.right, TOP_BAR_H};
    side = get_side_panel_draw_rect(client);
    bottom = (RECT){client.left, client.bottom - BOTTOM_BAR_H,
                    client.right, client.bottom};
    top_invalidations = rect_overlap_area(band, top) > 0;
    side_invalidations = rect_overlap_area(band, side) > 0;
    bottom_invalidations = rect_overlap_area(band, bottom) > 0;
    render_context_end();
done:
    fprintf(summary,
        "case=world_announcement_partial_present ok=%d partial_path=%d underlay_restores=%d static_rebuilds=%d top_invalidations=%d side_invalidations=%d bottom_invalidations=%d\n",
        partial && restored == 1 && after == before && !top_invalidations &&
        !side_invalidations && !bottom_invalidations, partial, restored,
        after - before, top_invalidations, side_invalidations, bottom_invalidations);
    if (surface.dc) surface_destroy(&surface);
    free(snapshot);
    world_announcement_queue_reset();
    world_announcement_store_clear();
    restore_ui_state(saved);
    return partial && restored == 1 && after == before && !top_invalidations &&
           !side_invalidations && !bottom_invalidations;
}

static int toast_coexistence_case(FILE *summary) {
    RECT client = {0, 0, 1366, 768};
    ProbeUiState saved = save_ui_state();
    RECT band, toast;
    int overlap, gap;
    side_panel_w = 380;
    side_panel_collapsed = 0;
    band = get_world_announcement_rect(client);
    toast = ui_notifications_rect_for_announcement(client, 1);
    overlap = rect_overlap_area(band, toast);
    gap = toast.top - band.bottom;
    restore_ui_state(saved);
    fprintf(summary,
        "case=world_announcement_toast_coexistence ok=%d both_visible=%d overlap_pixels=%d gap=%d\n",
        !overlap && gap == 8 && !IsRectEmpty(&band) && !IsRectEmpty(&toast),
        !IsRectEmpty(&band) && !IsRectEmpty(&toast), overlap, gap);
    return !overlap && gap == 8 && !IsRectEmpty(&band) && !IsRectEmpty(&toast);
}

int game_presentation_world_announcement_layout_probe(
    FILE *summary, const WorldAnnouncementProbeBundle *bundle) {
    int ok = map_overlay_layout_case(summary);
    ok &= topbar_preserved_case(summary, &bundle->age);
    ok &= alpha_typography_cases(summary, &bundle->age);
    ok &= underlay_freshness_case(summary, &bundle->age);
    ok &= partial_present_case(summary, &bundle->age);
    ok &= toast_coexistence_case(summary);
    return ok;
}
