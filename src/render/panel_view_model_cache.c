#include "render/panel_view_model_cache.h"

#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "ui/ui_layout.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int valid;
    unsigned int data_key;
    unsigned int ui_key;
    DWORD built_tick;
    int build_ms;
    int refresh_count;
} PanelViewCache;

static PanelViewCache panel_cache;
static int force_refresh = 1;
static int panel_reason_counts[4];
static int panel_last_reason;
static const char *panel_reason_names[4] = {"force", "invalid", "ui", "data"};

static RECT panel_rect_for(RECT client) {
    RECT panel;
    RECT handle;
    panel.left = side_panel_collapsed ? client.right - SIDE_PANEL_COLLAPSED_W : client.right - side_panel_w;
    panel.top = TOP_BAR_H;
    panel.right = client.right;
    panel.bottom = client.bottom;
    handle = get_side_panel_handle_rect(client);
    if (handle.left < panel.left) panel.left = handle.left;
    return panel;
}

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int panel_ui_key(RECT client) {
    unsigned int key = 2166136261u;
    key = mix_key(key, panel_tab);
    key = mix_key(key, selected_civ);
    key = mix_key(key, ui_language);
    key = mix_key(key, display_mode);
    key = mix_key(key, side_panel_w);
    key = mix_key(key, side_panel_collapsed);
    key = mix_key(key, country_detail_subtab);
    key = mix_key(key, country_decision_subtab);
    key = mix_key(key, country_diplomacy_view);
    key = mix_key(key, country_show_fallen);
    key = mix_key(key, country_list_scroll_offset);
    key = mix_key(key, country_detail_scroll_offset);
    key = mix_key(key, worldgen_scroll_offset);
    key = mix_key(key, pending_map_size);
    key = mix_key(key, debug_subtab);
    key = mix_key(key, debug_system_scroll_offset);
    key = mix_key(key, debug_event_filter);
    key = mix_key(key, debug_event_log_scroll_offset);
    key = mix_key(key, ocean_slider);
    key = mix_key(key, continent_slider);
    key = mix_key(key, relief_slider);
    key = mix_key(key, moisture_slider);
    key = mix_key(key, drought_slider);
    key = mix_key(key, vegetation_slider);
    key = mix_key(key, bias_forest_slider);
    key = mix_key(key, bias_desert_slider);
    key = mix_key(key, bias_mountain_slider);
    key = mix_key(key, bias_wetland_slider);
    key = mix_key(key, region_size_slider);
    key = mix_key(key, plague_fog_alpha);
    key = mix_key(key, initial_civ_count);
    key = mix_key(key, (int)selected_civ_color);
    key = mix_key(key, selected_civ_color_index);
    key = mix_key(key, map_zoom_percent);
    key = mix_key(key, map_legend_collapsed);
    key = mix_key(key, client.right - client.left);
    key = mix_key(key, client.bottom - client.top);
    return key;
}

static unsigned int panel_data_key(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    unsigned int key = snapshot ? snapshot->revision : 0;
    key = mix_key(key, render_snapshot_revision());
    return key;
}

static void release_panel_cache(void) {
    if (panel_cache.dc && panel_cache.old_bitmap) SelectObject(panel_cache.dc, panel_cache.old_bitmap);
    if (panel_cache.bitmap) DeleteObject(panel_cache.bitmap);
    if (panel_cache.dc) DeleteDC(panel_cache.dc);
    memset(&panel_cache, 0, sizeof(panel_cache));
    force_refresh = 1;
}

static int ensure_panel_cache(HDC hdc, RECT client) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return 0;
    if (panel_cache.dc && panel_cache.width == width && panel_cache.height == height) return 1;
    release_panel_cache();
    panel_cache.dc = CreateCompatibleDC(hdc);
    panel_cache.bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!panel_cache.dc || !panel_cache.bitmap) {
        release_panel_cache();
        return 0;
    }
    panel_cache.old_bitmap = SelectObject(panel_cache.dc, panel_cache.bitmap);
    panel_cache.width = width;
    panel_cache.height = height;
    return 1;
}

static int should_rebuild(unsigned int ui_key, unsigned int data_key) {
    DWORD now = GetTickCount();
    DWORD interval = panel_tab == PANEL_DEBUG ? 1000u : 125u;
    if (force_refresh) { panel_last_reason = 0; return 1; }
    if (!panel_cache.valid) { panel_last_reason = 1; return 1; }
    if (panel_cache.ui_key != ui_key) { panel_last_reason = 2; return 1; }
    if (panel_cache.data_key == data_key) return 0;
    if (now - panel_cache.built_tick >= interval) { panel_last_reason = 3; return 1; }
    return 0;
}

static void rebuild_panel_cache(HDC hdc, RECT client, RECT panel,
                                unsigned int ui_key, unsigned int data_key) {
    DWORD start = GetTickCount();
    (void)hdc;
    fill_rect(panel_cache.dc, panel, ui_theme_color(UI_COLOR_PANEL));
    draw_side_panel(panel_cache.dc, client);
    panel_cache.ui_key = ui_key;
    panel_cache.data_key = data_key;
    panel_cache.built_tick = GetTickCount();
    panel_cache.build_ms = (int)(panel_cache.built_tick - start);
    panel_cache.refresh_count++;
    panel_reason_counts[panel_last_reason]++;
    panel_cache.valid = 1;
    force_refresh = 0;
}

void panel_view_model_cache_draw(HDC hdc, RECT client) {
    RECT panel = panel_rect_for(client);
    unsigned int ui_key = panel_ui_key(client);
    unsigned int data_key = panel_data_key();
    if (!ensure_panel_cache(hdc, client)) {
        draw_side_panel(hdc, client);
        return;
    }
    if (should_rebuild(ui_key, data_key)) rebuild_panel_cache(hdc, client, panel, ui_key, data_key);
    BitBlt(hdc, panel.left, panel.top, panel.right - panel.left, panel.bottom - panel.top,
           panel_cache.dc, panel.left, panel.top, SRCCOPY);
}

void panel_view_model_cache_invalidate(void) {
    force_refresh = 1;
}

int panel_view_model_cache_last_build_ms(void) {
    return panel_cache.build_ms;
}

int panel_view_model_cache_age_ms(void) {
    if (!panel_cache.valid) return 0;
    return (int)(GetTickCount() - panel_cache.built_tick);
}

int panel_view_model_cache_refresh_count(void) {
    return panel_cache.refresh_count;
}

const char *panel_view_model_cache_last_reason(void) { return panel_reason_names[panel_last_reason]; }
const char *panel_view_model_cache_reason_summary(void) { static char text[96]; snprintf(text, sizeof(text), "force %d / ui %d / data %d", panel_reason_counts[0], panel_reason_counts[2], panel_reason_counts[3]); return text; }
