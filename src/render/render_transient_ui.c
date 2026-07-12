#include "render/render_transient_ui.h"

#include "core/game_types.h"
#include "render/pause_menu_render.h"
#include "render/render_context.h"
#include "render/top_notifications.h"
#include "render/top_world_announcement.h"
#include "render/top_world_announcement_resources.h"
#include "render/top_world_announcement_surface.h"
#include "ui/color_picker.h"
#include "ui/ui_layout.h"
#include "ui/ui_notifications.h"
#include "ui/world_announcement_queue.h"

#include <string.h>

static int underlay_captures;
static int underlay_restores;

static int rect_contains(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom &&
           inner.right > inner.left && inner.bottom > inner.top;
}

static TopWorldAnnouncementUnderlayKey underlay_key(RECT client) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    MapLayout layout = get_map_layout(client);
    TopWorldAnnouncementUnderlayKey key;
    memset(&key, 0, sizeof(key));
    key.client_width = client.right - client.left;
    key.client_height = client.bottom - client.top;
    key.side_panel_width = side_panel_w;
    key.side_panel_collapsed = side_panel_collapsed;
    key.display_mode = display_mode;
    key.language = ui_language;
    key.map_x = layout.map_x;
    key.map_y = layout.map_y;
    key.draw_width = layout.draw_w;
    key.draw_height = layout.draw_h;
    key.selected_civ = selected_civ;
    key.selected_x = selected_x;
    key.selected_y = selected_y;
    key.legend_collapsed = map_legend_collapsed;
    key.interaction_preview = map_interaction_preview;
    key.snapshot_revision = snapshot ? snapshot->revision : 0;
    return key;
}

int render_transient_ui_prepare(HDC hdc, RECT client, int progress_active) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    const WorldAnnouncementEvent *current = world_announcement_queue_current();
    RECT band = get_world_announcement_rect(client);
    int previous_event_id = current ? current->event_id : 0;
    top_world_announcement_resources_ensure();
    top_world_announcement_resources_prewarm(hdc);
    top_world_announcement_surface_prewarm(hdc, band);
    if (progress_active) return 0;
    world_announcement_queue_consume(snapshot);
    current = world_announcement_queue_current();
    return previous_event_id != (current ? current->event_id : 0);
}

int render_transient_ui_has_visible(int progress_active) {
    if (progress_active) return 0;
    return world_announcement_queue_active() || ui_notifications_active() ||
           pause_menu_open || color_picker_active();
}

void render_transient_ui_draw_full(HDC hdc, RECT client, int progress_active) {
    RECT band = get_world_announcement_rect(client);
    TopWorldAnnouncementUnderlayKey key;
    render_transient_ui_prepare(hdc, client, progress_active);
    if (progress_active) return;
    if (world_announcement_queue_active() && !IsRectEmpty(&band)) {
        key = underlay_key(client);
        if (top_world_announcement_surface_capture_underlay(hdc, band, &key)) {
            underlay_captures++;
        }
        draw_top_world_announcement(hdc, client);
    }
    draw_top_notifications(hdc, client);
    if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
    color_picker_draw(hdc, client);
}

int render_transient_ui_can_partial(RECT client, RECT paint) {
    RECT band = get_world_announcement_rect(client);
    TopWorldAnnouncementUnderlayKey key = underlay_key(client);
    return rect_contains(band, paint) &&
           top_world_announcement_surface_underlay_matches(band, &key);
}

int render_transient_ui_draw_partial(HDC hdc, RECT client, RECT paint) {
    RECT band = get_world_announcement_rect(client);
    TopWorldAnnouncementUnderlayKey key = underlay_key(client);
    if (!rect_contains(band, paint) ||
        !top_world_announcement_surface_underlay_matches(band, &key)) return 0;
    underlay_restores++;
    if (world_announcement_queue_active() &&
        draw_top_world_announcement(hdc, client)) return 1;
    return top_world_announcement_surface_restore_underlay(hdc, band, &key);
}

void render_transient_ui_reset_probe_metrics(void) {
    underlay_captures = 0;
    underlay_restores = 0;
}

int render_transient_ui_underlay_captures(void) { return underlay_captures; }
int render_transient_ui_underlay_restores(void) { return underlay_restores; }
