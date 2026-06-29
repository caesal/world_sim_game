#include "render/panel_map_speed_badge.h"

#include "core/constants.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/panel_debug.h"
#include "render/render_common.h"
#include "ui/ui_alliance_panel_input.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

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

static int text_width(const char *text) {
    HDC screen = GetDC(NULL);
    SIZE size;
    measure_text_utf8(screen, text, &size);
    ReleaseDC(NULL, screen);
    return size.cx;
}

static int render_speed_badge_bmp(const char *path, int language, int auto_running,
                                  int render_ms, int pending) {
    const int width = 1100, height = 420;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RECT map = get_map_viewport_rect(client);
    MapLayout layout = get_map_layout(client);
    RECT content = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w,
                    layout.map_y + layout.draw_h};
    RECT bottom = {0, height - BOTTOM_BAR_H, width, height};
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
    fill_rect(hdc, client, RGB(17, 24, 28));
    fill_rect(hdc, map, RGB(75, 151, 202));
    fill_rect(hdc, content, RGB(132, 154, 86));
    fill_rect(hdc, bottom, RGB(20, 26, 30));
    panel_map_draw_actual_speed_badge(hdc, client, 40);
    panel_map_draw_bottom_status_chips(hdc, client, 0, language, render_ms, pending, auto_running, 0);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    return ok;
}

static int chip_pair_fits(RECT rect, const char *label, const char *value) {
    return text_width(label) + 6 + text_width(value) <= (rect.right - rect.left) - 26;
}

int game_presentation_map_speed_probe(FILE *summary) {
    PanelMapBottomStatus en, en99, en999, en_render999, zh, zh99, zh999, zh_render999, paused;
    char badge[24], empty[24], combined[160];
    RECT client = {0, 0, 1280, 720};
    RECT rect, viewport;
    MapLayout layout;
    int old_collapsed = side_panel_collapsed;
    int old_side_w = side_panel_w;
    int old_zoom = map_zoom_percent;
    int old_x = map_offset_x;
    int old_y = map_offset_y;
    int artifact_ok;
    int ok = 1;
    int passive_hit;
    int passive_scope_blocked;
    int union_filter;
    unsigned int fail_mask = 0;

    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;

    panel_map_bottom_status_build(&en, client, 0, UI_LANG_EN, 67, 0, 1, 0);
    panel_map_bottom_status_build(&en99, client, 0, UI_LANG_EN, 67, 99, 1, 0);
    panel_map_bottom_status_build(&en999, client, 0, UI_LANG_EN, 67, 999, 1, 0);
    panel_map_bottom_status_build(&en_render999, client, 0, UI_LANG_EN, 999, 0, 1, 0);
    panel_map_bottom_status_build(&zh, client, 0, UI_LANG_ZH, 67, 0, 1, 0);
    panel_map_bottom_status_build(&zh99, client, 0, UI_LANG_ZH, 67, 99, 1, 0);
    panel_map_bottom_status_build(&zh999, client, 0, UI_LANG_ZH, 67, 999, 1, 0);
    panel_map_bottom_status_build(&zh_render999, client, 0, UI_LANG_ZH, 999, 0, 1, 0);
    panel_map_bottom_status_build(&paused, client, 0, UI_LANG_EN, 22, 3, 0, 0);
    panel_map_format_actual_speed_badge(badge, sizeof(badge), 40);
    panel_map_format_actual_speed_badge(empty, sizeof(empty), 0);
    rect = panel_map_actual_speed_badge_rect(client);
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);

    snprintf(combined, sizeof(combined), "%s %s | %s %s | %s",
             en.render_label, en.render_value, en.queue_label, en.queue_value,
             en.status_text);
    if (!(strcmp(en.render_label, "Render") == 0 && strcmp(en.render_value, "67ms") == 0)) fail_mask |= 1u << 0;
    if (!(strcmp(en.queue_label, "Queue") == 0 && strcmp(en.queue_value, "0") == 0)) fail_mask |= 1u << 1;
    if (!(strcmp(en.status_text, "Stable") == 0 && strcmp(paused.status_text, "Paused") == 0)) fail_mask |= 1u << 2;
    if (!(strcmp(zh.render_label, "渲染") == 0 && strcmp(zh.render_value, "67ms") == 0)) fail_mask |= 1u << 3;
    if (!(strcmp(zh.queue_label, "队列") == 0 && strcmp(zh.queue_value, "0") == 0)) fail_mask |= 1u << 4;
    if (strcmp(zh.status_text, "稳定") != 0) fail_mask |= 1u << 5;
    if (!(strstr(combined, "Target") == NULL && strstr(combined, "Actual") == NULL)) fail_mask |= 1u << 6;
    if (!(strstr(combined, "40ms") == NULL && strcmp(badge, "40ms") == 0)) fail_mask |= 1u << 7;
    if (!(strcmp(empty, "--ms") == 0 && panel_map_actual_speed_badge_inside_frame(client))) fail_mask |= 1u << 8;
    if (!(rect.left == viewport.left + 12 && rect.top == viewport.top + 10)) fail_mask |= 1u << 9;
    if (!(layout.map_x > viewport.left + 40 && rect.left < layout.map_x)) fail_mask |= 1u << 10;
    if (!(en.queue_rect.left > en.render_rect.right && en.status_rect.left > en.queue_rect.right)) fail_mask |= 1u << 11;
    if (text_width("Overloaded") > (en.status_rect.right - en.status_rect.left - 46)) fail_mask |= 1u << 12;
    if (!(chip_pair_fits(en.queue_rect, en.queue_label, en.queue_value) &&
          chip_pair_fits(en99.queue_rect, en99.queue_label, en99.queue_value) &&
          chip_pair_fits(en999.queue_rect, en999.queue_label, en999.queue_value))) fail_mask |= 1u << 13;
    if (!(chip_pair_fits(en.render_rect, en.render_label, en.render_value) &&
          chip_pair_fits(en_render999.render_rect, en_render999.render_label, en_render999.render_value))) fail_mask |= 1u << 14;
    if (!(chip_pair_fits(zh.queue_rect, zh.queue_label, zh.queue_value) &&
          chip_pair_fits(zh99.queue_rect, zh99.queue_label, zh99.queue_value) &&
          chip_pair_fits(zh999.queue_rect, zh999.queue_label, zh999.queue_value))) fail_mask |= 1u << 15;
    if (!(chip_pair_fits(zh.render_rect, zh.render_label, zh.render_value) &&
          chip_pair_fits(zh_render999.render_rect, zh_render999.render_label, zh_render999.render_value))) fail_mask |= 1u << 16;
    if (!(strcmp(en99.queue_value, "99") == 0 && strcmp(en999.queue_value, "999") == 0)) fail_mask |= 1u << 17;
    if (!(strcmp(zh99.queue_value, "99") == 0 && strcmp(zh999.queue_value, "999") == 0)) fail_mask |= 1u << 18;
    if (!(strcmp(en_render999.render_value, "999ms") == 0 &&
          strcmp(zh_render999.render_value, "999ms") == 0)) fail_mask |= 1u << 19;
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    diplomacy_score_tooltip_register_bar((RECT){10, 10, 90, 18}, 0, 1);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    passive_hit = ui_alliance_panel_passive_tooltip_hit(20, 14);
    if (!passive_hit) fail_mask |= 1u << 21;
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY);
    diplomacy_score_tooltip_register_bar((RECT){10, 10, 90, 18}, 0, 1);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY);
    passive_scope_blocked = !ui_alliance_panel_passive_tooltip_hit(20, 14);
    if (!passive_scope_blocked) fail_mask |= 1u << 22;
    union_filter = debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                               DEBUG_EVENT_FILTER_WAR_DIPLOMACY);
    if (!union_filter ||
        !debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                     DEBUG_EVENT_FILTER_ALL) ||
        debug_panel_probe_event_type_matches_filter(EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                                                    DEBUG_EVENT_FILTER_COLLAPSE_PLAGUE)) {
        fail_mask |= 1u << 23;
    }
    artifact_ok = render_speed_badge_bmp(PRESENTATION_PROBE_DIR "/map_speed_badge_bottom_status_en.bmp",
                                         UI_LANG_EN, 0, 999, 999);
    artifact_ok &= render_speed_badge_bmp(PRESENTATION_PROBE_DIR "/map_speed_badge_bottom_status_zh.bmp",
                                          UI_LANG_ZH, 1, 999, 999);
    if (!artifact_ok) fail_mask |= 1u << 20;
    ok = fail_mask == 0;

    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side_w;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    fprintf(summary,
            "case=map_speed_status ok=%d fail_mask=0x%x render=%s/%s queue=%s/%s q99=%s q999=%s status=%s/%s badge=%s empty=%s chips=%ld,%ld,%ld rect=%ld,%ld,%ld,%ld viewport=%ld,%ld map_x=%d passive_bar=%d union_filter=%d artifact=%d files=map_speed_badge_bottom_status_en.bmp/map_speed_badge_bottom_status_zh.bmp\n",
            ok, fail_mask, en.render_label, en.render_value, en.queue_label, en.queue_value,
            en99.queue_value, en999.queue_value, zh.status_text, paused.status_text, badge, empty,
            en.render_rect.left, en.queue_rect.left, en.status_rect.left,
            rect.left, rect.top, rect.right, rect.bottom, viewport.left, viewport.top,
            layout.map_x, passive_hit && passive_scope_blocked, union_filter, artifact_ok);
    return ok;
}
