#include "ui/ui_world_announcement.h"

#include "core/game_types.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_notifications.h"
#include "ui/ui_selection.h"
#include "ui/ui_snapshot_read.h"
#include "ui/world_announcement_queue.h"

static int last_hovered;
static int last_control = -1;

static void invalidate_active_transition(HWND hwnd, int before, int after) {
    RECT client;
    RECT dirty;
    RECT toast;
    if (!hwnd) return;
    GetClientRect(hwnd, &client);
    dirty = get_world_announcement_rect(client);
    if (ui_notifications_active()) {
        toast = ui_notifications_rect_for_announcement(client, before);
        UnionRect(&dirty, &dirty, &toast);
        toast = ui_notifications_rect_for_announcement(client, after);
        UnionRect(&dirty, &dirty, &toast);
    }
    InvalidateRect(hwnd, &dirty, FALSE);
}

RECT ui_world_announcement_dirty_rect(RECT client) {
    return get_world_announcement_rect(client);
}

void ui_world_announcement_invalidate(HWND hwnd) {
    RECT client;
    RECT rect;
    if (!hwnd) return;
    GetClientRect(hwnd, &client);
    rect = ui_world_announcement_dirty_rect(client);
    if (rect.right > rect.left) InvalidateRect(hwnd, &rect, FALSE);
}

int ui_world_announcement_hover_control(RECT client, int x, int y) {
    int i;
    if (!world_announcement_queue_active()) return -1;
    for (i = WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS;
         i <= WORLD_ANNOUNCEMENT_CONTROL_DISMISS; i++) {
        if (point_in_rect(get_world_announcement_control_rect(
                client, (WorldAnnouncementControl)i), x, y)) return i;
    }
    return -1;
}

static void locate_current(HWND hwnd) {
    const WorldAnnouncementEvent *event = world_announcement_queue_current();
    int civ_id;
    if (!event) return;
    civ_id = event->location_civ_id;
    if (civ_id < 0 || ui_snapshot_civ_uid(civ_id) != event->location_civ_uid) return;
    if (ui_locate_civ(hwnd, civ_id)) {
        ui_invalidate_map_viewport(hwnd);
        ui_invalidate_side_panel(hwnd);
    }
}

int ui_world_announcement_handle_click(HWND hwnd, RECT client, int x, int y) {
    int control = ui_world_announcement_hover_control(client, x, y);
    int was_active = world_announcement_queue_active();
    if (control < 0) return 0;
    if (control == WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS &&
        world_announcement_queue_page_count() > 1) {
        world_announcement_queue_previous_page();
    } else if (control == WORLD_ANNOUNCEMENT_CONTROL_NEXT &&
               world_announcement_queue_page_count() > 1) {
        world_announcement_queue_next_page();
    } else if (control == WORLD_ANNOUNCEMENT_CONTROL_LOCATE) {
        locate_current(hwnd);
    } else if (control == WORLD_ANNOUNCEMENT_CONTROL_DISMISS) {
        world_announcement_queue_dismiss();
    }
    if (was_active != world_announcement_queue_active())
        invalidate_active_transition(hwnd, was_active, world_announcement_queue_active());
    else ui_world_announcement_invalidate(hwnd);
    return 1;
}

void ui_world_announcement_update_hover(HWND hwnd, RECT client, int x, int y) {
    RECT band = get_world_announcement_rect(client);
    int hovered = world_announcement_queue_active() && point_in_rect(band, x, y);
    int control = ui_world_announcement_hover_control(client, x, y);
    world_announcement_queue_set_hovered(hovered);
    if (hovered != last_hovered || control != last_control) {
        last_hovered = hovered;
        last_control = control;
        ui_world_announcement_invalidate(hwnd);
    }
}

void ui_world_announcement_mouse_leave(HWND hwnd) {
    world_announcement_queue_set_hovered(0);
    if (last_hovered || last_control >= 0) {
        last_hovered = 0;
        last_control = -1;
        ui_world_announcement_invalidate(hwnd);
    }
}

void ui_world_announcement_tick(HWND hwnd, DWORD now) {
    int was_active = world_announcement_queue_active();
    if (world_announcement_queue_tick(now)) {
        int is_active = world_announcement_queue_active();
        if (was_active != is_active) invalidate_active_transition(hwnd, was_active, is_active);
        else ui_world_announcement_invalidate(hwnd);
    }
}
