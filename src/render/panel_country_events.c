#include "render/panel_country_events.h"

#include "core/render_snapshot.h"
#include "render/render_context.h"
#include "render/render_common.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

#define COUNTRY_EVENT_VISIBLE_CARDS 5
#define COUNTRY_EVENT_CARD_SLOT_H 64
#define COUNTRY_EVENT_AREA_H (COUNTRY_EVENT_VISIBLE_CARDS * COUNTRY_EVENT_CARD_SLOT_H + 12)
#define COUNTRY_EVENT_HIT_MAX 48

static RECT recent_events_rect;
static int recent_events_rect_valid;
static int recent_scroll_offset;
static int recent_scroll_match_count;
static int recent_scroll_visible_count;
static int recent_last_civ = -1;
static RECT recent_hit_rects[COUNTRY_EVENT_HIT_MAX];
static int recent_hit_civs[COUNTRY_EVENT_HIT_MAX];
static int recent_hit_uids[COUNTRY_EVENT_HIT_MAX];
static int recent_hit_count;

void country_recent_events_reset_hit(void) {
    recent_events_rect_valid = 0;
    recent_hit_count = 0;
}

static int source_event_count(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    if (!snapshot || recent_last_civ < 0) return 0;
    return render_snapshot_civ_recent_event_count(snapshot, recent_last_civ);
}

static int source_event_entry(int index, EventLogEntry *out) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    if (!snapshot || recent_last_civ < 0) return 0;
    return render_snapshot_civ_recent_event_get_entry(snapshot, recent_last_civ, index, out);
}

static int recent_event_count_for_civ(int civ_id) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return snapshot ? render_snapshot_civ_recent_event_count(snapshot, civ_id) : 0;
}

static int text_height_utf8(HDC hdc, const char *text, int width) {
    WCHAR wide[EVENT_LOG_LEN * 3];
    RECT rect = {0, 0, max(80, width), 0};
    int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide,
                                  (int)(sizeof(wide) / sizeof(wide[0])));
    if (len <= 0) return 18;
    DrawTextW(hdc, wide, -1, &rect, DT_WORDBREAK | DT_CALCRECT);
    return max(18, rect.bottom - rect.top);
}

static COLORREF event_accent(EventLogSeverity severity) {
    switch (severity) {
        case EVENT_SEVERITY_DANGER: return RGB(202, 84, 75);
        case EVENT_SEVERITY_WARNING: return RGB(218, 166, 78);
        default: return RGB(92, 145, 175);
    }
}

static void add_country_hit(RECT rect, int civ_id, int uid) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count || uid <= 0 ||
        recent_hit_count >= COUNTRY_EVENT_HIT_MAX) return;
    recent_hit_rects[recent_hit_count] = rect;
    recent_hit_civs[recent_hit_count] = civ_id;
    recent_hit_uids[recent_hit_count] = uid;
    recent_hit_count++;
}

static int event_param_a_is_civ_local(EventLogType type) {
    return type == EVENT_TYPE_VASSAL_TRANSFERRED;
}

static void draw_country_chip(HDC hdc, RECT *line, int civ_id, const EventCivSnapshot *snapshot) {
    char text[96];
    SIZE size;
    RECT chip;
    if (!snapshot || snapshot->uid <= 0 || line->left >= line->right - 18) return;
    snprintf(text, sizeof(text), "%c %.48s", snapshot->symbol,
             ui_language == UI_LANG_ZH ? snapshot->name_zh : snapshot->name_en);
    measure_text_utf8(hdc, text, &size);
    chip = (RECT){line->left, line->top, min(line->left + size.cx + 20, line->right), line->top + 22};
    if (chip.right <= chip.left + 10) return;
    fill_rect_alpha(hdc, chip, snapshot->color, 112);
    draw_text_rect(hdc, (RECT){chip.left + 6, chip.top, chip.right - 6, chip.bottom}, text,
                   RGB(246, 248, 250), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    add_country_hit(chip, civ_id, snapshot->uid);
    line->left = chip.right + 6;
}

static void draw_country_chips(HDC hdc, RECT line, const EventLogEntry *entry) {
    draw_country_chip(hdc, &line, entry->civ_id, &entry->civ_snapshot);
    if (entry->target_id != entry->civ_id) {
        draw_country_chip(hdc, &line, entry->target_id, &entry->target_snapshot);
    }
    if (event_param_a_is_civ_local(entry->type) &&
        entry->param_a != entry->civ_id && entry->param_a != entry->target_id) {
        draw_country_chip(hdc, &line, entry->param_a, &entry->param_a_snapshot);
    }
}

static int event_card_height(HDC hdc, const EventLogEntry *entry, int width) {
    char text[EVENT_LOG_LEN * 3];
    event_log_format_entry_data(entry, ui_language, text, sizeof(text));
    return clamp(34 + text_height_utf8(hdc, text, width - 24), 58, 132);
}

static int draw_event_card(HDC hdc, UiCursor *cursor, const EventLogEntry *entry) {
    char text[EVENT_LOG_LEN * 3];
    RECT card;
    RECT stripe;
    RECT body;
    int h = event_card_height(hdc, entry, cursor->width - 10);
    if (cursor->y > cursor->bottom - h) return 0;
    card = ui_take_rect(cursor, h);
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe = card;
    stripe.right = stripe.left + 4;
    fill_rect(hdc, stripe, event_accent(entry->severity));
    draw_country_chips(hdc, (RECT){card.left + 9, card.top + 5, card.right - 8, card.top + 27}, entry);
    body = (RECT){card.left + 9, card.top + 30, card.right - 8, card.bottom - 5};
    event_log_format_entry_data(entry, ui_language, text, sizeof(text));
    draw_text_rect(hdc, body, text, ui_theme_color(UI_COLOR_TEXT), DT_WORDBREAK | DT_END_ELLIPSIS);
    cursor->y += 6;
    return 1;
}

static void draw_recent_scrollbar(HDC hdc, int shown) {
    RECT track;
    RECT thumb;
    int track_h;
    int thumb_h;
    int max_offset;
    recent_scroll_visible_count = shown;
    if (!recent_events_rect_valid || recent_scroll_match_count <= shown || shown <= 0) return;
    track = (RECT){recent_events_rect.right - 7, recent_events_rect.top,
                   recent_events_rect.right - 2, recent_events_rect.bottom};
    track_h = max(1, track.bottom - track.top);
    thumb_h = clamp(track_h * shown / max(1, recent_scroll_match_count), 24, track_h);
    max_offset = max(1, recent_scroll_match_count - shown);
    thumb = track;
    thumb.top += (track_h - thumb_h) * clamp(recent_scroll_offset, 0, max_offset) / max_offset;
    thumb.bottom = thumb.top + thumb_h;
    fill_rect_alpha(hdc, track, RGB(10, 12, 14), 96);
    fill_rect(hdc, thumb, RGB(112, 122, 126));
}

void draw_country_recent_events(HDC hdc, UiCursor *cursor, int civ_id) {
    EventLogEntry entry;
    UiCursor list;
    HRGN clip;
    int i;
    int skipped = 0;
    int shown = 0;
    int area_h = COUNTRY_EVENT_AREA_H;
    int available_h;

    ui_section(hdc, cursor, tr("Recent Events", "近期事件"));
    if (recent_last_civ != civ_id) {
        recent_scroll_offset = 0;
        recent_last_civ = civ_id;
    }
    recent_hit_count = 0;
    recent_scroll_match_count = recent_event_count_for_civ(civ_id);
    recent_scroll_offset = clamp(recent_scroll_offset, 0, max(0, recent_scroll_match_count - 1));
    available_h = max(0, cursor->bottom - cursor->y - 8);
    if (available_h <= 0) return;
    area_h = min(area_h, available_h);
    recent_events_rect = ui_take_rect(cursor, area_h);
    recent_events_rect_valid = 1;
    fill_rect_alpha(hdc, recent_events_rect, RGB(24, 29, 32), 96);
    list = ui_cursor(recent_events_rect.left + 6, recent_events_rect.top + 6,
                     recent_events_rect.right - recent_events_rect.left - 16,
                     recent_events_rect.bottom - 6);
    clip = CreateRectRgn(recent_events_rect.left, recent_events_rect.top,
                         recent_events_rect.right, recent_events_rect.bottom);
    SelectClipRgn(hdc, clip);
    for (i = 0; i < source_event_count() && list.y < list.bottom - 50; i++) {
        if (!source_event_entry(i, &entry)) continue;
        if (skipped < recent_scroll_offset) {
            skipped++;
            continue;
        }
        if (draw_event_card(hdc, &list, &entry)) shown++;
        else break;
    }
    if (recent_scroll_match_count == 0) {
        draw_text_rect(hdc, recent_events_rect,
                       tr("No related events in the current history window.",
                          "当前记录窗口内暂无相关事件。"),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);
    draw_recent_scrollbar(hdc, shown);
    cursor->y += 8;
}

int country_recent_events_scroll_hit_test(int mouse_x, int mouse_y) {
    return recent_events_rect_valid && point_in_rect(recent_events_rect, mouse_x, mouse_y);
}

int country_recent_events_scroll(int item_delta) {
    int old_offset = recent_scroll_offset;
    recent_scroll_offset = clamp(recent_scroll_offset + item_delta, 0, max(0, recent_scroll_match_count - 1));
    return recent_scroll_offset != old_offset;
}

int country_recent_events_country_hit_test(int mouse_x, int mouse_y) {
    int i;
    for (i = recent_hit_count - 1; i >= 0; i--) {
        int civ_id = recent_hit_civs[i];
        const RenderSnapshot *snapshot;
        int owned_snapshot = 0;
        int valid;
        if (!point_in_rect(recent_hit_rects[i], mouse_x, mouse_y)) continue;
        snapshot = render_context_snapshot();
        if (!snapshot) {
            snapshot = render_snapshot_acquire();
            owned_snapshot = 1;
        }
        valid = snapshot && civ_id >= 0 && civ_id < snapshot->civ_count &&
                snapshot->civs[civ_id].uid == recent_hit_uids[i];
        if (owned_snapshot) render_snapshot_release(snapshot);
        if (valid) return civ_id;
        return -1;
    }
    return -1;
}
