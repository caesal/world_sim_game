#include "render/diplomacy_map_anim.h"

#include "core/country_focus.h"
#include "core/game_state.h"
#include "render/render_common.h"

#include <stdlib.h>
#include <string.h>

#define DIPLO_ANIM_MAX 5
#define DIPLO_ANIM_MS 3500

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
} DiplomacyMapAnim;

static DiplomacyMapAnim animations[DIPLO_ANIM_MAX];
static int next_slot;
static int last_seen_total;

static int focus_for_event_civ(int civ_id, int uid, int *x, int *y) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    if (uid > 0 && civs[civ_id].uid != uid) return 0;
    return country_focus_point(civ_id, x, y);
}

static int event_endpoint(const EventLogEntry *entry, int use_target, int *x, int *y) {
    if (!use_target) return focus_for_event_civ(entry->civ_id, entry->civ_uid, x, y);
    if (entry->type == EVENT_TYPE_VASSAL_TRANSFERRED && entry->param_a >= 0) {
        return focus_for_event_civ(entry->param_a, entry->param_a_uid, x, y);
    }
    return focus_for_event_civ(entry->target_id, entry->target_uid, x, y);
}

static void event_target_identity(const EventLogEntry *entry, int *id, int *uid) {
    if (entry->type == EVENT_TYPE_VASSAL_TRANSFERRED && entry->param_a >= 0) {
        *id = entry->param_a;
        *uid = entry->param_a_uid;
        return;
    }
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
    } else if (type == EVENT_TYPE_WAR_STARTED) {
        *color = RGB(205, 62, 52);
        *icon = ICON_ATTACK;
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
           type == EVENT_TYPE_WAR_STARTED ||
           type == EVENT_TYPE_TRUCE_SIGNED ||
           type == EVENT_TYPE_WAR_FRONT_SEVERED ||
           type == EVENT_TYPE_VASSAL_CREATED ||
           type == EVENT_TYPE_VASSAL_RELEASED ||
           type == EVENT_TYPE_VASSAL_TRANSFERRED ||
           type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE;
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
    return &animations[next_slot++ % DIPLO_ANIM_MAX];
}

static void enqueue_event_anim(const EventLogEntry *entry) {
    DiplomacyMapAnim *anim;
    int x1, y1, x2, y2;
    int to_id, to_uid;
    if (!entry || !anim_type(entry->type)) return;
    if (!event_endpoint(entry, 0, &x1, &y1) || !event_endpoint(entry, 1, &x2, &y2)) return;
    event_target_identity(entry, &to_id, &to_uid);
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
}

void diplomacy_map_anim_consume_events(void) {
    int total = event_log_total_entries;
    int delta;
    int i;
    if (last_seen_total == 0) {
        last_seen_total = total;
        return;
    }
    delta = total - last_seen_total;
    if (delta <= 0) return;
    if (delta > EVENT_LOG_COUNT) delta = EVENT_LOG_COUNT;
    if (delta > 12) delta = 12;
    for (i = delta - 1; i >= 0; i--) {
        EventLogEntry entry;
        if (event_log_get_entry(i, &entry)) enqueue_event_anim(&entry);
    }
    last_seen_total = total;
}

static POINT map_point(MapLayout layout, int x, int y) {
    POINT p;
    p.x = layout.map_x + (x * 2 + 1) * layout.draw_w / (MAP_W * 2);
    p.y = layout.map_y + (y * 2 + 1) * layout.draw_h / (MAP_H * 2);
    return p;
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

static void draw_anim(HDC hdc, MapLayout layout, const DiplomacyMapAnim *anim, DWORD now) {
    POINT p1 = map_point(layout, anim->x1, anim->y1);
    POINT p2 = map_point(layout, anim->x2, anim->y2);
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
    lift = clamp(dist * 18 / 100, 24, 90);
    cx = mx - dy * lift * anim->normal_sign / dist;
    cy = my + dx * lift * anim->normal_sign / dist;
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

void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout) {
    DWORD now = GetTickCount();
    int i;
    (void)client;
    for (i = 0; i < DIPLO_ANIM_MAX; i++) {
        if (!animations[i].active) continue;
        if ((int)(now - animations[i].start_ms) > DIPLO_ANIM_MS) {
            animations[i].active = 0;
            continue;
        }
        draw_anim(hdc, layout, &animations[i], now);
    }
}

int diplomacy_map_anim_active(void) {
    DWORD now = GetTickCount();
    int i;
    for (i = 0; i < DIPLO_ANIM_MAX; i++) {
        if (animations[i].active && (int)(now - animations[i].start_ms) <= DIPLO_ANIM_MS) return 1;
    }
    return 0;
}
