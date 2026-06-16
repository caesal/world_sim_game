#include "render/map_highlight.h"

#include "core/game_state.h"
#include "render/map_highlight_internal.h"
#include "render/map_presentation_policy.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"

enum {
    HIGHLIGHT_PRIORITY_DIM = 1,
    HIGHLIGHT_PRIORITY_ALLIANCE = 40,
    HIGHLIGHT_PRIORITY_VASSAL = 42,
    HIGHLIGHT_PRIORITY_WAR = 44,
    HIGHLIGHT_PRIORITY_OVERLORD = 45,
    HIGHLIGHT_PRIORITY_SECONDARY = 50,
    HIGHLIGHT_PRIORITY_PRIMARY = 80
};

static int last_selected_request_present;
static int last_alliance_request_count;
static int last_war_request_count;
static int last_vassal_request_count;

static const RenderSnapshot *highlight_snapshot(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return snapshot && snapshot->world_generated ? snapshot : NULL;
}

int map_highlight_valid_civ(const RenderSnapshot *snapshot, int civ_id) {
    return snapshot && civ_id >= 0 && civ_id < snapshot->civ_count && snapshot->civs[civ_id].alive;
}

static int highlight_civ(const RenderSnapshot *snapshot) {
    if (map_highlight_valid_civ(snapshot, map_highlight_civ)) return map_highlight_civ;
    if (map_highlight_valid_civ(snapshot, selected_civ)) return selected_civ;
    return -1;
}

COLORREF map_highlight_mix_color(COLORREF a, COLORREF b, int b_percent) {
    int ap = clamp(100 - b_percent, 0, 100);
    int bp = clamp(b_percent, 0, 100);
    return RGB((GetRValue(a) * ap + GetRValue(b) * bp) / 100,
               (GetGValue(a) * ap + GetGValue(b) * bp) / 100,
               (GetBValue(a) * ap + GetBValue(b) * bp) / 100);
}

static COLORREF civ_color(const RenderSnapshot *snapshot, int civ_id) {
    if (!map_highlight_valid_civ(snapshot, civ_id)) return RGB(238, 220, 146);
    return (COLORREF)snapshot->civs[civ_id].color;
}

COLORREF map_highlight_civ_highlight_color(const RenderSnapshot *snapshot, int civ_id, int strong) {
    return map_highlight_mix_color(civ_color(snapshot, civ_id), RGB(255, 255, 245), strong ? 36 : 52);
}

COLORREF map_highlight_civ_shadow_color(const RenderSnapshot *snapshot, int civ_id) {
    return map_highlight_mix_color(civ_color(snapshot, civ_id), RGB(24, 22, 18), 68);
}

static COLORREF alliance_highlight_color(void) {
    return RGB(86, 152, 218);
}

static COLORREF relation_border_color(const RenderSnapshot *snapshot, int civ_id, int strong) {
    COLORREF own = map_highlight_civ_highlight_color(snapshot, civ_id, strong);
    if (!map_highlight_valid_civ(snapshot, selected_civ) || civ_id == selected_civ) return own;
    if (snapshot->relations[selected_civ][civ_id].state == DIPLOMACY_WAR ||
        snapshot->wars[selected_civ][civ_id].active) return RGB(214, 70, 58);
    if (snapshot->civs[civ_id].overlord == selected_civ) return RGB(232, 200, 86);
    if (snapshot->civs[selected_civ].overlord == civ_id) return RGB(176, 112, 218);
    if (snapshot->relations[selected_civ][civ_id].state == DIPLOMACY_ALLIANCE) {
        return alliance_highlight_color();
    }
    return own;
}

static int visible_bounds_snapshot(const RenderSnapshot *snapshot, RECT client, MapLayout layout,
                                   int *min_x, int *max_x, int *min_y, int *max_y) {
    RECT viewport = get_map_viewport_rect(client);
    int left = max(viewport.left, layout.map_x);
    int top = max(viewport.top, layout.map_y);
    int right = min(viewport.right, layout.map_x + layout.draw_w);
    int bottom = min(viewport.bottom, layout.map_y + layout.draw_h);
    if (!snapshot || layout.draw_w <= 0 || layout.draw_h <= 0 || right <= left || bottom <= top) return 0;
    *min_x = clamp((left - layout.map_x) * snapshot->map_w / layout.draw_w - 1, 0, snapshot->map_w - 1);
    *max_x = clamp((right - layout.map_x) * snapshot->map_w / layout.draw_w + 1, 0, snapshot->map_w - 1);
    *min_y = clamp((top - layout.map_y) * snapshot->map_h / layout.draw_h - 1, 0, snapshot->map_h - 1);
    *max_y = clamp((bottom - layout.map_y) * snapshot->map_h / layout.draw_h + 1, 0, snapshot->map_h - 1);
    return 1;
}

static unsigned int premultiplied_pixel(COLORREF color, BYTE alpha) {
    unsigned int a = alpha;
    unsigned int r = (unsigned int)GetRValue(color) * a / 255u;
    unsigned int g = (unsigned int)GetGValue(color) * a / 255u;
    unsigned int b = (unsigned int)GetBValue(color) * a / 255u;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void fill_request(HighlightRequest *request, const RenderSnapshot *snapshot,
                         int civ_id, int primary, int secondary, int dim,
                         int strong, int pulse_start, COLORREF fill_color,
                         BYTE alpha, int priority) {
    COLORREF inner = relation_border_color(snapshot, civ_id, strong);
    request->civ_id = civ_id;
    request->primary = primary;
    request->secondary = secondary;
    request->dim = dim;
    request->priority = priority;
    request->strong = strong;
    request->width_boost = snapshot ? map_presentation_highlight_width_boost(snapshot->map_w,
                                                                             snapshot->map_h) : 0;
    request->pulse_start = pulse_start;
    request->pixel = premultiplied_pixel(fill_color, alpha);
    request->inner = inner;
    request->outer = map_highlight_mix_color(inner, RGB(18, 16, 14), 72);
}

static void add_highlight_request(const RenderSnapshot *snapshot, HighlightRequest *requests,
                                  int *count, int civ_id, int primary, int secondary,
                                  int dim, int strong, int pulse_start,
                                  COLORREF fill_color, BYTE alpha, int priority) {
    int i, lowest = -1;
    if (!requests || !count) return;
    for (i = 0; i < *count; i++) {
        if (requests[i].dim == dim && requests[i].civ_id == civ_id) {
            if (priority >= requests[i].priority) {
                fill_request(&requests[i], snapshot, civ_id, primary, secondary, dim,
                             strong, pulse_start, fill_color, alpha, priority);
            }
            return;
        }
    }
    if (*count >= HIGHLIGHT_REQUEST_MAX) {
        for (i = 0; i < *count; i++) {
            if (lowest < 0 || requests[i].priority < requests[lowest].priority) lowest = i;
        }
        if (lowest < 0 || priority <= requests[lowest].priority) return;
        fill_request(&requests[lowest], snapshot, civ_id, primary, secondary, dim,
                     strong, pulse_start, fill_color, alpha, priority);
        return;
    }
    fill_request(&requests[*count], snapshot, civ_id, primary, secondary, dim,
                 strong, pulse_start, fill_color, alpha, priority);
    (*count)++;
}

static void record_request_summary(const HighlightRequest *requests, int count) {
    int i;
    last_selected_request_present = 0;
    last_alliance_request_count = 0;
    last_war_request_count = 0;
    last_vassal_request_count = 0;
    for (i = 0; i < count; i++) {
        if (!requests[i].dim && requests[i].civ_id == selected_civ &&
            requests[i].priority == HIGHLIGHT_PRIORITY_PRIMARY) last_selected_request_present = 1;
        if (requests[i].priority == HIGHLIGHT_PRIORITY_ALLIANCE) last_alliance_request_count++;
        if (requests[i].priority == HIGHLIGHT_PRIORITY_WAR) last_war_request_count++;
        if (requests[i].priority == HIGHLIGHT_PRIORITY_VASSAL ||
            requests[i].priority == HIGHLIGHT_PRIORITY_OVERLORD) last_vassal_request_count++;
    }
}

static int primary_is_strong(const RenderSnapshot *snapshot, int primary) {
    return primary == map_highlight_civ || !map_highlight_valid_civ(snapshot, map_highlight_civ);
}

static void collect_relation_requests(const RenderSnapshot *snapshot, HighlightRequest *requests,
                                      int *request_count) {
    int i;
    int overlord;
    if (!map_highlight_valid_civ(snapshot, selected_civ)) return;
    overlord = snapshot->civs[selected_civ].overlord;
    if (map_highlight_valid_civ(snapshot, overlord)) {
        add_highlight_request(snapshot, requests, request_count, overlord, -1, -1, 0, 0,
                              selected_civ_pulse_start_ms,
                              map_highlight_civ_highlight_color(snapshot, overlord, 0), 58,
                              HIGHLIGHT_PRIORITY_OVERLORD);
        for (i = 0; i < snapshot->civ_count; i++) {
            if (i != selected_civ && snapshot->civs[i].overlord == overlord) {
                add_highlight_request(snapshot, requests, request_count, i, -1, -1, 0, 0, 0,
                                      map_highlight_civ_highlight_color(snapshot, i, 0), 58,
                                      HIGHLIGHT_PRIORITY_VASSAL);
            }
        }
    } else {
        for (i = 0; i < snapshot->civ_count; i++) {
            if (snapshot->civs[i].overlord == selected_civ) {
                add_highlight_request(snapshot, requests, request_count, i, -1, -1, 0, 0,
                                      selected_civ_pulse_start_ms,
                                      map_highlight_civ_highlight_color(snapshot, i, 0), 58,
                                      HIGHLIGHT_PRIORITY_VASSAL);
            }
        }
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        if (i != selected_civ && snapshot->civs[i].alive &&
            (snapshot->relations[selected_civ][i].state == DIPLOMACY_WAR ||
             snapshot->wars[selected_civ][i].active)) {
            add_highlight_request(snapshot, requests, request_count, i, -1, -1, 0, 0, 0,
                                  map_highlight_civ_highlight_color(snapshot, i, 0), 58,
                                  HIGHLIGHT_PRIORITY_WAR);
        }
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        if (i != selected_civ && snapshot->civs[i].alive &&
            snapshot->relations[selected_civ][i].state == DIPLOMACY_ALLIANCE &&
            !snapshot->wars[selected_civ][i].active) {
            add_highlight_request(snapshot, requests, request_count, i, -1, -1, 0, 0, 0,
                                  alliance_highlight_color(), 62, HIGHLIGHT_PRIORITY_ALLIANCE);
        }
    }
}

void draw_country_highlight(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = highlight_snapshot();
    HighlightRequest requests[HIGHLIGHT_REQUEST_MAX];
    int primary = highlight_civ(snapshot);
    int secondary = map_highlight_valid_civ(snapshot, map_highlight_civ) &&
                    map_highlight_valid_civ(snapshot, selected_civ) &&
                    selected_civ != map_highlight_civ ? selected_civ : -1;
    int min_x, max_x, min_y, max_y, request_count = 0;
    map_highlight_debug_begin();
    if (!snapshot || primary < 0 ||
        !visible_bounds_snapshot(snapshot, client, layout, &min_x, &max_x, &min_y, &max_y)) {
        map_highlight_debug_end();
        return;
    }
    if (map_highlight_valid_civ(snapshot, map_highlight_civ)) {
        add_highlight_request(snapshot, requests, &request_count, -1, primary, secondary, 1, 0, 0,
                              RGB(0, 0, 0), 18, HIGHLIGHT_PRIORITY_DIM);
    }
    if (secondary >= 0) {
        add_highlight_request(snapshot, requests, &request_count, secondary, -1, -1, 0, 0,
                              selected_civ_pulse_start_ms,
                              map_highlight_civ_highlight_color(snapshot, secondary, 0), 58,
                              HIGHLIGHT_PRIORITY_SECONDARY);
    }
    collect_relation_requests(snapshot, requests, &request_count);
    add_highlight_request(snapshot, requests, &request_count, primary, -1, -1, 0,
                          primary_is_strong(snapshot, primary),
                          primary == map_highlight_civ ? map_highlight_pulse_start_ms : selected_civ_pulse_start_ms,
                          map_highlight_civ_highlight_color(snapshot, primary,
                                                            primary_is_strong(snapshot, primary)),
                          (BYTE)(primary_is_strong(snapshot, primary) ? 112 : 58),
                          HIGHLIGHT_PRIORITY_PRIMARY);
    record_request_summary(requests, request_count);
    map_highlight_blend_overlay_requests(hdc, client, layout, snapshot, min_x, max_x, min_y, max_y,
                                         requests, request_count);
    map_highlight_draw_edge_focus_requests(hdc, client, layout, snapshot, min_x, max_x, min_y, max_y,
                                           requests, request_count);
    map_highlight_debug_end();
}

int map_highlight_last_selected_request_present(void) { return last_selected_request_present; }
int map_highlight_last_alliance_request_count(void) { return last_alliance_request_count; }
int map_highlight_last_war_request_count(void) { return last_war_request_count; }
int map_highlight_last_vassal_request_count(void) { return last_vassal_request_count; }
