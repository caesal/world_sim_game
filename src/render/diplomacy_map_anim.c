#include "render/diplomacy_map_anim.h"

#include "render/render_common.h"
#include "sim/diplomacy.h"
#include "ui/ui_layout.h"

#include <stdlib.h>
#include <string.h>

#define DIPLO_ANIM_MAX 16
#define DIPLO_ANIM_MS 3500
#define DIPLO_ANIM_RECENT_MONTHS 18
#define DIPLO_EVENT_SCAN_MAX 32

typedef struct {
    int active;
    EventLogType type;
    int x1, y1, x2, y2;
    DWORD start_ms;
    COLORREF color;
    IconId icon;
    int bidirectional;
    int from_id;
    int from_uid;
    int to_id;
    int to_uid;
    int normal_sign;
    int map_w;
    int map_h;
    int drawn_once;
} DiplomacyMapAnim;

static DiplomacyMapAnim animations[DIPLO_ANIM_MAX];
static int next_slot;
static int last_consumed_total;
static int consumed_initialized;
static unsigned int last_consumed_snapshot_revision;
static int last_consumed_events_revision;
static int delayed_waiting_for_snapshot;
static int stale_prevented_count;
static int enqueued_count, overwritten_count, endpoint_reject_count;
static int contact_reject_count, contact_gate_bypassed_count, war_front_gate_bypassed_count;
static int drawn_count, expired_before_draw_count, cached_paint_blocked_count;

static int focus_for_event_civ(const RenderSnapshot *snapshot, int civ_id, int uid, int *x, int *y) {
    const SnapshotCiv *civ;
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return 0;
    civ = &snapshot->civs[civ_id];
    if (!civ->alive || !civ->focus_valid) return 0;
    if (uid > 0 && civ->uid != uid) return 0;
    if (x) *x = civ->focus_x;
    if (y) *y = civ->focus_y;
    return 1;
}

static int event_endpoint(const RenderSnapshot *snapshot, const EventLogEntry *entry,
                          int use_target, int *x, int *y) {
    if (!use_target) return focus_for_event_civ(snapshot, entry->civ_id, entry->civ_uid, x, y);
    if (entry->type == EVENT_TYPE_VASSAL_TRANSFERRED && entry->param_a >= 0) return focus_for_event_civ(snapshot, entry->param_a, entry->param_a_uid, x, y);
    return focus_for_event_civ(snapshot, entry->target_id, entry->target_uid, x, y);
}

static void event_target_identity(const EventLogEntry *entry, int *id, int *uid) {
    if (entry->type == EVENT_TYPE_VASSAL_TRANSFERRED && entry->param_a >= 0) { *id = entry->param_a; *uid = entry->param_a_uid; return; }
    *id = entry->target_id;
    *uid = entry->target_uid;
}

static void anim_style(EventLogType type, COLORREF *color, IconId *icon, int *bidirectional) {
    *bidirectional = 1;
    *color = RGB(82, 190, 104);
    *icon = ICON_COHESION;
    if (type == EVENT_TYPE_DIPLOMACY_TENSE) {
        *color = RGB(220, 150, 62);
        *icon = ICON_DISORDER;
    } else if (type == EVENT_TYPE_DIPLOMACY_ALLIANCE) {
        *color = RGB(86, 152, 218);
        *icon = ICON_COHESION;
    } else if (type == EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED) {
        *color = RGB(150, 118, 192);
        *icon = ICON_COUNTRY_DEFENSE;
    } else if (type == EVENT_TYPE_WAR_STARTED) {
        *color = RGB(205, 62, 52);
        *icon = ICON_ATTACK;
        *bidirectional = 0;
    } else if (type == EVENT_TYPE_TRUCE_SIGNED || type == EVENT_TYPE_WAR_FRONT_SEVERED) {
        *color = RGB(220, 178, 72);
        *icon = ICON_COUNTRY_DEFENSE;
    } else if (type == EVENT_TYPE_VASSAL_CREATED || type == EVENT_TYPE_VASSAL_TRANSFERRED) {
        *color = RGB(174, 116, 214);
        *icon = ICON_GOVERNANCE;
        *bidirectional = 0;
    } else if (type == EVENT_TYPE_VASSAL_RELEASED || type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE) {
        *color = RGB(232, 184, 82);
        *icon = ICON_TERRITORY;
        *bidirectional = 0;
    }
}

static int anim_type(EventLogType type) {
    return type == EVENT_TYPE_DIPLOMACY_PEACE ||
           type == EVENT_TYPE_DIPLOMACY_TENSE ||
           type == EVENT_TYPE_DIPLOMACY_ALLIANCE ||
           type == EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED ||
           type == EVENT_TYPE_WAR_STARTED ||
           type == EVENT_TYPE_TRUCE_SIGNED ||
           type == EVENT_TYPE_WAR_FRONT_SEVERED ||
           type == EVENT_TYPE_VASSAL_CREATED ||
           type == EVENT_TYPE_VASSAL_RELEASED ||
           type == EVENT_TYPE_VASSAL_TRANSFERRED ||
           type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE;
}

static int contact_required_anim(EventLogType type) {
    return type == EVENT_TYPE_DIPLOMACY_PEACE ||
           type == EVENT_TYPE_DIPLOMACY_TENSE ||
           type == EVENT_TYPE_DIPLOMACY_ALLIANCE ||
           type == EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED;
}

static int valid_snapshot_pair(const RenderSnapshot *snapshot, int from_id, int to_id) { return snapshot && from_id >= 0 && to_id >= 0 && from_id < snapshot->civ_count && to_id < snapshot->civ_count; }

static int event_anim_contact_valid(const RenderSnapshot *snapshot, EventLogType type,
                                    int from_id, int to_id) {
    if (!valid_snapshot_pair(snapshot, from_id, to_id)) return 0;
    if (type == EVENT_TYPE_WAR_STARTED) {
        if (!snapshot->war_front_flags[from_id][to_id] &&
            !snapshot->war_front_flags[to_id][from_id]) {
            war_front_gate_bypassed_count++;
        }
        return 1;
    }
    if (type == EVENT_TYPE_DIPLOMACY_PEACE || type == EVENT_TYPE_DIPLOMACY_TENSE) {
        if (snapshot->relations[from_id][to_id].contact_kind == DIP_CONTACT_NONE &&
            snapshot->relations[to_id][from_id].contact_kind == DIP_CONTACT_NONE) {
            contact_gate_bypassed_count++;
        }
        return 1;
    }
    if (contact_required_anim(type)) {
        return snapshot->relations[from_id][to_id].contact_kind != DIP_CONTACT_NONE ||
               snapshot->relations[to_id][from_id].contact_kind != DIP_CONTACT_NONE;
    }
    return 1;
}

static int identity_match(int a_id, int a_uid, int b_id, int b_uid) {
    if (a_id != b_id) return 0;
    return a_uid <= 0 || b_uid <= 0 || a_uid == b_uid;
}

static int same_anim_pair(const DiplomacyMapAnim *anim, int from_id, int from_uid, int to_id, int to_uid) {
    if (!anim->active) return 0;
    if (identity_match(anim->from_id, anim->from_uid, from_id, from_uid) &&
        identity_match(anim->to_id, anim->to_uid, to_id, to_uid)) return 1;
    return identity_match(anim->from_id, anim->from_uid, to_id, to_uid) &&
           identity_match(anim->to_id, anim->to_uid, from_id, from_uid);
}

static DiplomacyMapAnim *animation_slot_for(int from_id, int from_uid, int to_id, int to_uid) {
    int i;
    for (i = 0; i < DIPLO_ANIM_MAX; i++) {
        if (same_anim_pair(&animations[i], from_id, from_uid, to_id, to_uid)) return &animations[i];
    }
    {
        DiplomacyMapAnim *slot = &animations[next_slot++ % DIPLO_ANIM_MAX];
        if (slot->active) overwritten_count++;
        return slot;
    }
}

static void enqueue_event_anim(const RenderSnapshot *snapshot, const EventLogEntry *entry) {
    DiplomacyMapAnim *anim;
    int x1, y1, x2, y2;
    int to_id, to_uid;
    if (!entry || !anim_type(entry->type)) return;
    event_target_identity(entry, &to_id, &to_uid);
    if (!event_anim_contact_valid(snapshot, entry->type, entry->civ_id, to_id)) {
        contact_reject_count++;
        return;
    }
    if (!event_endpoint(snapshot, entry, 0, &x1, &y1) ||
        !event_endpoint(snapshot, entry, 1, &x2, &y2)) {
        endpoint_reject_count++;
        return;
    }
    anim = animation_slot_for(entry->civ_id, entry->civ_uid, to_id, to_uid);
    memset(anim, 0, sizeof(*anim));
    anim->active = 1;
    anim->type = entry->type;
    anim->x1 = x1;
    anim->y1 = y1;
    anim->x2 = x2;
    anim->y2 = y2;
    anim->start_ms = GetTickCount();
    anim_style(entry->type, &anim->color, &anim->icon, &anim->bidirectional);
    anim->from_id = entry->civ_id;
    anim->from_uid = entry->civ_uid;
    anim->to_id = to_id;
    anim->to_uid = to_uid;
    anim->normal_sign = ((entry->civ_id * 31 + to_id * 17 + entry->type) & 1) ? 1 : -1;
    anim->map_w = snapshot->map_w;
    anim->map_h = snapshot->map_h;
    enqueued_count++;
}

static int event_would_enqueue(const RenderSnapshot *snapshot, const EventLogEntry *entry) {
    int x1, y1, x2, y2;
    int to_id, to_uid;
    if (!snapshot || !entry || !anim_type(entry->type)) return 0;
    event_target_identity(entry, &to_id, &to_uid);
    if (!event_anim_contact_valid(snapshot, entry->type, entry->civ_id, to_id)) return 0;
    return event_endpoint(snapshot, entry, 0, &x1, &y1) &&
           event_endpoint(snapshot, entry, 1, &x2, &y2);
}

static int event_is_recent_for_snapshot(const RenderSnapshot *snapshot,
                                        const EventLogEntry *entry) {
    int event_index, snapshot_index, delta;
    if (!snapshot || !entry) return 0;
    if (entry->year <= 0 || entry->month <= 0 || snapshot->year <= 0 || snapshot->month <= 0) return 0;
    event_index = entry->year * 12 + entry->month;
    snapshot_index = snapshot->year * 12 + snapshot->month;
    delta = snapshot_index - event_index;
    return delta >= 0 && delta <= DIPLO_ANIM_RECENT_MONTHS;
}

static int first_snapshot_event_delta(const RenderSnapshot *snapshot) { return snapshot ? min(snapshot->event_count, DIPLO_EVENT_SCAN_MAX) : 0; }

int diplomacy_map_anim_pending_events(const RenderSnapshot *snapshot) {
    int delta, i;
    if (!snapshot || !snapshot->world_generated) return 0;
    if (!consumed_initialized) delta = first_snapshot_event_delta(snapshot);
    else {
        delta = snapshot->event_total_entries - last_consumed_total;
        if (delta <= 0) return delayed_waiting_for_snapshot;
        if (delta > snapshot->event_count) delta = snapshot->event_count;
        if (delta > DIPLO_EVENT_SCAN_MAX) delta = DIPLO_EVENT_SCAN_MAX;
    }
    for (i = delta - 1; i >= 0; i--) {
        EventLogEntry entry;
        if (render_snapshot_event_get_entry(snapshot, i, &entry) &&
            (consumed_initialized || event_is_recent_for_snapshot(snapshot, &entry)) &&
            event_would_enqueue(snapshot, &entry)) return 1;
    }
    return delayed_waiting_for_snapshot;
}

void diplomacy_map_anim_delay_for_snapshot(const RenderSnapshot *snapshot) {
    delayed_waiting_for_snapshot = diplomacy_map_anim_pending_events(snapshot);
    if (delayed_waiting_for_snapshot) stale_prevented_count++;
}

void diplomacy_map_anim_consume_events(const RenderSnapshot *snapshot) {
    int total;
    int delta;
    int i;
    delayed_waiting_for_snapshot = 0;
    if (!snapshot) return;
    total = snapshot->event_total_entries;
    if (!consumed_initialized) {
        consumed_initialized = 1;
        last_consumed_snapshot_revision = snapshot->revision;
        last_consumed_events_revision = snapshot->events_revision;
        delta = first_snapshot_event_delta(snapshot);
        for (i = delta - 1; i >= 0; i--) {
            EventLogEntry entry;
            if (render_snapshot_event_get_entry(snapshot, i, &entry) &&
                event_is_recent_for_snapshot(snapshot, &entry)) {
                enqueue_event_anim(snapshot, &entry);
            }
        }
        last_consumed_total = total;
        return;
    }
    delta = total - last_consumed_total;
    if (delta <= 0) return;
    if (delta > snapshot->event_count) delta = snapshot->event_count;
    if (delta > DIPLO_EVENT_SCAN_MAX) delta = DIPLO_EVENT_SCAN_MAX;
    for (i = delta - 1; i >= 0; i--) {
        EventLogEntry entry;
        if (render_snapshot_event_get_entry(snapshot, i, &entry)) enqueue_event_anim(snapshot, &entry);
    }
    last_consumed_total = total;
    last_consumed_snapshot_revision = snapshot->revision;
    last_consumed_events_revision = snapshot->events_revision;
}

static int valid_anim_for_snapshot(const DiplomacyMapAnim *anim, const RenderSnapshot *snapshot) {
    if (!anim || !anim->active || !snapshot || !snapshot->world_generated) return 0;
    if (snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    if (anim->map_w != snapshot->map_w || anim->map_h != snapshot->map_h) return 0;
    if (anim->x1 < 0 || anim->x1 >= snapshot->map_w || anim->x2 < 0 || anim->x2 >= snapshot->map_w) return 0;
    if (anim->y1 < 0 || anim->y1 >= snapshot->map_h || anim->y2 < 0 || anim->y2 >= snapshot->map_h) return 0;
    return 1;
}

static POINT map_point(MapLayout layout, const RenderSnapshot *snapshot, int x, int y) {
    POINT p;
    p.x = layout.map_x + (x * 2 + 1) * layout.draw_w / (max(1, snapshot->map_w) * 2);
    p.y = layout.map_y + (y * 2 + 1) * layout.draw_h / (max(1, snapshot->map_h) * 2);
    return p;
}

static int map_anim_clip_rect(RECT client, MapLayout layout, RECT *out) {
    RECT viewport = get_map_viewport_rect(client);
    RECT map_rect = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};
    RECT clip = {max(viewport.left, map_rect.left), max(viewport.top, map_rect.top),
                 min(viewport.right, map_rect.right), min(viewport.bottom, map_rect.bottom)};
    if (clip.right <= clip.left || clip.bottom <= clip.top) return 0;
    if (out) *out = clip;
    return 1;
}

static COLORREF mix_color(COLORREF a, COLORREF b, int percent_b) {
    int percent_a = 100 - percent_b;
    int r = (GetRValue(a) * percent_a + GetRValue(b) * percent_b) / 100;
    int g = (GetGValue(a) * percent_a + GetGValue(b) * percent_b) / 100;
    int bl = (GetBValue(a) * percent_a + GetBValue(b) * percent_b) / 100;
    return RGB(r, g, bl);
}

static void offset_capital_endpoints(MapLayout layout, POINT *p1, POINT *p2) {
    int dx = p2->x - p1->x;
    int dy = p2->y - p1->y;
    int dist = max(1, abs(dx) + abs(dy));
    int icon_radius = clamp(layout.tile_size * 2 + 8, 13, 24);
    int start_gap = icon_radius + 10;
    int end_gap = icon_radius + 14;
    int available = dist - 28;
    int total = start_gap + end_gap;
    if (available <= 0) {
        start_gap = max(0, dist / 6);
        end_gap = max(0, dist / 6);
    } else if (total > available) {
        start_gap = max(4, start_gap * available / total);
        end_gap = max(4, end_gap * available / total);
    }
    p1->x += dx * start_gap / dist;
    p1->y += dy * start_gap / dist;
    p2->x -= dx * end_gap / dist;
    p2->y -= dy * end_gap / dist;
}

static void draw_arrow_tip(HDC hdc, POINT from, POINT to, COLORREF color, int width) {
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    int dist = max(1, abs(dx) + abs(dy));
    int len = 18;
    int wing = 10;
    POINT base = {to.x - dx * len / dist, to.y - dy * len / dist};
    POINT a = {base.x - dy * wing / dist, base.y + dx * wing / dist};
    POINT b = {base.x + dy * wing / dist, base.y - dx * wing / dist};
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    MoveToEx(hdc, a.x, a.y, NULL);
    LineTo(hdc, to.x, to.y);
    LineTo(hdc, b.x, b.y);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_icon_marker(HDC hdc, POINT center, COLORREF color, IconId icon) {
    RECT outer = {center.x - 17, center.y - 17, center.x + 17, center.y + 17};
    RECT inner = {center.x - 11, center.y - 11, center.x + 11, center.y + 11};
    HBRUSH brush = CreateSolidBrush(mix_color(color, RGB(26, 24, 22), 28));
    HPEN pen = CreatePen(PS_SOLID, 1, mix_color(color, RGB(34, 28, 24), 55));
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    Ellipse(hdc, outer.left, outer.top, outer.right, outer.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
    draw_icon(hdc, icon, inner, RGB(250, 244, 220));
}

static int clamp_to_span(int value, int lo, int hi) {
    if (hi < lo) return lo;
    return clamp(value, lo, hi);
}

static void draw_anim(HDC hdc, MapLayout layout, const RenderSnapshot *snapshot,
                      const RECT *clip, const DiplomacyMapAnim *anim, DWORD now) {
    POINT p1 = map_point(layout, snapshot, anim->x1, anim->y1);
    POINT p2 = map_point(layout, snapshot, anim->x2, anim->y2);
    POINT pts[24];
    int mx, my, dx, dy, dist, lift, cx, cy;
    int elapsed = (int)(now - anim->start_ms);
    int i;
    int icon_offset;
    COLORREF glow_color;
    HPEN pen;
    HGDIOBJ old_pen;
    POINT icon_center;
    if (elapsed < 0 || elapsed > DIPLO_ANIM_MS) return;
    offset_capital_endpoints(layout, &p1, &p2);
    mx = (p1.x + p2.x) / 2;
    my = (p1.y + p2.y) / 2;
    dx = p2.x - p1.x;
    dy = p2.y - p1.y;
    dist = max(1, abs(dx) + abs(dy));
    lift = clamp(dist * 8 / 100, 12, 44);
    cx = mx - dy * lift * anim->normal_sign / dist;
    cy = my + dx * lift * anim->normal_sign / dist;
    if (clip) {
        cx = clamp_to_span(cx, clip->left + 2, clip->right - 2);
        cy = clamp_to_span(cy, clip->top + 2, clip->bottom - 2);
    }
    for (i = 0; i < 24; i++) {
        int t = i * 1000 / 23;
        int omt = 1000 - t;
        pts[i].x = (omt * omt * p1.x + 2 * omt * t * cx + t * t * p2.x) / 1000000;
        pts[i].y = (omt * omt * p1.y + 2 * omt * t * cy + t * t * p2.y) / 1000000;
    }
    glow_color = mix_color(anim->color, RGB(38, 34, 30), 40);
    pen = CreatePen(PS_SOLID, elapsed < 650 ? 8 : 6, glow_color);
    old_pen = SelectObject(hdc, pen);
    Polyline(hdc, pts, 24);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
    pen = CreatePen(PS_SOLID, elapsed < 650 ? 4 : 3, anim->color);
    old_pen = SelectObject(hdc, pen);
    Polyline(hdc, pts, 24);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
    draw_arrow_tip(hdc, pts[18], pts[23], anim->color, 4);
    if (anim->bidirectional) draw_arrow_tip(hdc, pts[5], pts[0], anim->color, 4);
    icon_offset = clamp(dist / 9, 10, 16);
    icon_center.x = ((p1.x + 2 * cx + p2.x) / 4) -
                    dy * icon_offset * anim->normal_sign / dist;
    icon_center.y = ((p1.y + 2 * cy + p2.y) / 4) +
                    dx * icon_offset * anim->normal_sign / dist;
    draw_icon_marker(hdc, icon_center, anim->color, anim->icon);
}

void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    DWORD now = GetTickCount();
    RECT clip;
    int saved;
    int i;
    if (!map_anim_clip_rect(client, layout, &clip)) return;
    saved = SaveDC(hdc);
    if (saved <= 0) return;
    IntersectClipRect(hdc, clip.left, clip.top, clip.right, clip.bottom);
    for (i = 0; i < DIPLO_ANIM_MAX; i++) {
        if (!animations[i].active) continue;
        if ((int)(now - animations[i].start_ms) > DIPLO_ANIM_MS) {
            if (!animations[i].drawn_once) expired_before_draw_count++;
            animations[i].active = 0;
            continue;
        }
        if (!valid_anim_for_snapshot(&animations[i], snapshot)) {
            animations[i].active = 0;
            continue;
        }
        draw_anim(hdc, layout, snapshot, &clip, &animations[i], now);
        if (!animations[i].drawn_once) { animations[i].drawn_once = 1; drawn_count++; }
    }
    RestoreDC(hdc, saved);
}

int diplomacy_map_anim_active_count(void) {
    DWORD now = GetTickCount();
    int i, count = 0;
    for (i = 0; i < DIPLO_ANIM_MAX; i++) {
        if (animations[i].active && (int)(now - animations[i].start_ms) <= DIPLO_ANIM_MS) count++;
    }
    return count;
}

int diplomacy_map_anim_active(void) { return diplomacy_map_anim_active_count() > 0; }
int diplomacy_map_anim_requires_dynamic_paint(const RenderSnapshot *snapshot) { return diplomacy_map_anim_active() || diplomacy_map_anim_delayed_waiting_for_snapshot() || diplomacy_map_anim_pending_events(snapshot); }
const char *diplomacy_map_anim_source(void) { return "snapshot"; }
int diplomacy_map_anim_delayed_waiting_for_snapshot(void) { return delayed_waiting_for_snapshot; }
int diplomacy_map_anim_last_consumed_total(void) { return last_consumed_total; }
unsigned int diplomacy_map_anim_last_snapshot_revision(void) { return last_consumed_snapshot_revision; }
int diplomacy_map_anim_last_events_revision(void) { return last_consumed_events_revision; }
int diplomacy_map_anim_stale_prevented_count(void) { return stale_prevented_count; }
int diplomacy_map_anim_enqueued_count(void) { return enqueued_count; }
int diplomacy_map_anim_drawn_count(void) { return drawn_count; }
int diplomacy_map_anim_expired_before_draw_count(void) { return expired_before_draw_count; }
int diplomacy_map_anim_overwritten_count(void) { return overwritten_count; }
int diplomacy_map_anim_endpoint_reject_count(void) { return endpoint_reject_count; }
int diplomacy_map_anim_contact_reject_count(void) { return contact_reject_count; }
int diplomacy_map_anim_gate_bypass_count(void) { return contact_gate_bypassed_count + war_front_gate_bypassed_count; }
void diplomacy_map_anim_note_cached_paint_blocked(void) { cached_paint_blocked_count++; }
int diplomacy_map_anim_cached_paint_blocked_count(void) { return cached_paint_blocked_count; }

void diplomacy_map_anim_debug_reset(void) {
    memset(animations, 0, sizeof(animations));
    next_slot = 0;
    last_consumed_total = consumed_initialized = 0;
    last_consumed_snapshot_revision = last_consumed_events_revision = 0;
    delayed_waiting_for_snapshot = stale_prevented_count = 0;
    enqueued_count = overwritten_count = endpoint_reject_count = 0;
    contact_reject_count = contact_gate_bypassed_count = war_front_gate_bypassed_count = 0;
    drawn_count = expired_before_draw_count = cached_paint_blocked_count = 0;
}
