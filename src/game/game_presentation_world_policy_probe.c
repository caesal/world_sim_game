#include "core/game_state.h"
#include "render/map_display_policy.h"
#include "render/panel_map_speed_badge.h"
#include "render/render_common.h"
#include "render/render_panel_internal.h"
#include "sim/alliance.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_DIR "build/validation/presentation_probe_20260618"

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER header = {0};
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static int make_surface(int w, int h, HDC *out_screen, HDC *out_hdc,
                        HBITMAP *out_bitmap, HBITMAP *out_old,
                        BITMAPINFO *info, void **bits) {
    memset(info, 0, sizeof(*info));
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = w;
    info->bmiHeader.biHeight = -h;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    *out_screen = GetDC(NULL);
    *out_hdc = CreateCompatibleDC(*out_screen);
    *out_bitmap = CreateDIBSection(*out_screen, info, DIB_RGB_COLORS, bits, NULL, 0);
    if (!*out_hdc || !*out_bitmap || !*bits) return 0;
    *out_old = SelectObject(*out_hdc, *out_bitmap);
    return 1;
}

static void free_surface(HDC screen, HDC hdc, HBITMAP bitmap, HBITMAP old) {
    SelectObject(hdc, old);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
}

static int text_fits(HDC hdc, RECT rect, const char *a, const char *b, int reserve) {
    SIZE sa = {0}, sb = {0};
    measure_text_utf8(hdc, a, &sa);
    measure_text_utf8(hdc, b, &sb);
    return sa.cx + sb.cx + reserve <= rect.right - rect.left;
}

static int render_bottom(const char *path, int language) {
    const int w = 1100, h = 180;
    HDC screen, hdc;
    HBITMAP bitmap, old;
    BITMAPINFO info;
    void *bits = NULL;
    RECT client = {0, 0, w, h};
    int old_language = ui_language;
    int old_collapsed = side_panel_collapsed;
    int ok;
    if (!make_surface(w, h, &screen, &hdc, &bitmap, &old, &info, &bits)) return 0;
    ui_language = language;
    side_panel_collapsed = 1;
    fill_rect(hdc, client, RGB(18, 24, 28));
    draw_bottom_bar(hdc, client);
    ok = write_bmp(path, &info, bits, w, h);
    ui_language = old_language;
    side_panel_collapsed = old_collapsed;
    free_surface(screen, hdc, bitmap, old);
    return ok;
}

static int case_bottom_row(FILE *summary) {
    RECT client = {0, 0, 1100, 720};
    RECT row = get_bottom_control_row_rect(client);
    RECT play = get_play_button_rect(client);
    PanelMapBottomStatus en, zh;
    RECT dot;
    HDC hdc = GetDC(NULL);
    int i, top_match = play.top == row.top, bottom_match = play.bottom == row.bottom;
    int text_en, text_zh, artifacts;
    for (i = 0; i < SPEED_COUNT; i++) {
        RECT speed = get_speed_button_rect(client, i);
        top_match &= speed.top == row.top;
        bottom_match &= speed.bottom == row.bottom;
    }
    panel_map_bottom_status_build(&en, client, 0, UI_LANG_EN, 999, 999, 1, 1);
    panel_map_bottom_status_build(&zh, client, 0, UI_LANG_ZH, 999, 999, 0, 0);
    top_match &= en.render_rect.top == row.top && en.queue_rect.top == row.top &&
                 en.status_rect.top == row.top && zh.render_rect.top == row.top &&
                 zh.queue_rect.top == row.top && zh.status_rect.top == row.top;
    bottom_match &= en.render_rect.bottom == row.bottom && en.queue_rect.bottom == row.bottom &&
                    en.status_rect.bottom == row.bottom && zh.render_rect.bottom == row.bottom &&
                    zh.queue_rect.bottom == row.bottom && zh.status_rect.bottom == row.bottom;
    dot = panel_map_bottom_status_dot_rect(&en);
    text_en = text_fits(hdc, en.render_rect, en.render_label, en.render_value, 34) &&
              text_fits(hdc, en.queue_rect, en.queue_label, en.queue_value, 34) &&
              text_fits(hdc, en.status_rect, en.status_text, "", 56);
    text_zh = text_fits(hdc, zh.render_rect, zh.render_label, zh.render_value, 34) &&
              text_fits(hdc, zh.queue_rect, zh.queue_label, zh.queue_value, 34) &&
              text_fits(hdc, zh.status_rect, zh.status_text, "", 56);
    ReleaseDC(NULL, hdc);
    artifacts = render_bottom(PROBE_DIR "/bottom_bar_shared_row_en.bmp", UI_LANG_EN) &&
                render_bottom(PROBE_DIR "/bottom_bar_shared_row_zh.bmp", UI_LANG_ZH);
    fprintf(summary,
        "case=bottom_bar_shared_row ok=%d height=%ld top_match=%d bottom_match=%d dot_centered=%d text_fit_en=%d text_fit_zh=%d\n",
        row.bottom - row.top == 30 && top_match && bottom_match &&
        (dot.top + dot.bottom) / 2 == (row.top + row.bottom) / 2 && text_en && text_zh && artifacts,
        row.bottom - row.top, top_match, bottom_match,
        (dot.top + dot.bottom) / 2 == (row.top + row.bottom) / 2, text_en, text_zh);
    return row.bottom - row.top == 30 && top_match && bottom_match && text_en && text_zh && artifacts;
}

static int render_alpha(const char *path, const char *label, MapDisplayFillPolicy fill) {
    const int w = 520, h = 180;
    HDC screen, hdc;
    HBITMAP bitmap, old;
    BITMAPINFO info;
    void *bits = NULL;
    RECT client = {0, 0, w, h}, swatch = {36, 40, w - 36, h - 36};
    char text[128];
    int ok;
    if (!make_surface(w, h, &screen, &hdc, &bitmap, &old, &info, &bits)) return 0;
    fill_rect(hdc, client, RGB(26, 38, 44));
    fill_rect(hdc, swatch, RGB(142, 154, 126));
    fill_rect_alpha(hdc, swatch, fill.color, (BYTE)fill.alpha);
    snprintf(text, sizeof(text), "%s  alpha=%d/255", label, fill.alpha);
    draw_center_text(hdc, swatch, text, RGB(250, 250, 248));
    ok = write_bmp(path, &info, bits, w, h);
    free_surface(screen, hdc, bitmap, old);
    return ok;
}

static void setup_alliance_state(AllianceSaveState *state, int member) {
    int i;
    memset(state, 0, sizeof(*state));
    for (i = 0; i < MAX_CIVS; i++) state->civ_alliance[i] = member ? 0 : -1;
    state->next_id = 1;
    state->records[0].active = 1;
    state->records[0].id = 0;
    state->records[0].founder_civ_id = 0;
    state->records[0].member_count = 1;
    state->records[0].members[0] = 0;
}

static int case_alpha_policy(FILE *summary) {
    RenderSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    AllianceSaveState *saved = malloc(sizeof(*saved));
    AllianceSaveState *state = malloc(sizeof(*state));
    Civilization old_civ0 = civs[0], old_civ1 = civs[1];
    int old_count = civ_count;
    MapDisplayFillPolicy sc, si, sm, lc, li, lm;
    int snapshot_ok, live_ok, artifacts;
    if (!snapshot || !saved || !state) { free(snapshot); free(saved); free(state); return 0; }
    snapshot->civ_count = 2;
    snapshot->civs[0].alive = snapshot->civs[1].alive = 1;
    snapshot->civs[0].color = COLOR32_RGB(42, 128, 222);
    snapshot->civs[1].color = COLOR32_RGB(206, 92, 76);
    snapshot->civs[0].alliance_display_id = 0;
    snapshot->civs[1].alliance_display_id = -1;
    snapshot->alliance_count = 1;
    snapshot->alliances[0].id = 0;
    snapshot->alliances[0].founder_civ_id = 0;
    snapshot->alliances[0].color = snapshot->civs[0].color;
    snapshot_ok = map_display_policy_snapshot_owner_fill(snapshot, 0, DISPLAY_POLITICAL, &sc) &&
                  map_display_policy_snapshot_owner_fill(snapshot, 1, DISPLAY_ALLIANCE, &si) &&
                  map_display_policy_snapshot_owner_fill(snapshot, 0, DISPLAY_ALLIANCE, &sm);
    alliance_copy_save_state(saved);
    civ_count = max(civ_count, 2);
    civs[0].alive = civs[1].alive = 1;
    civs[0].color = snapshot->civs[0].color;
    civs[1].color = snapshot->civs[1].color;
    setup_alliance_state(state, 1);
    alliance_restore_save_state(state);
    live_ok = map_display_policy_live_owner_fill(0, DISPLAY_POLITICAL, &lc) &&
              map_display_policy_live_owner_fill(0, DISPLAY_ALLIANCE, &lm);
    setup_alliance_state(state, 0);
    alliance_restore_save_state(state);
    live_ok &= map_display_policy_live_owner_fill(1, DISPLAY_ALLIANCE, &li);
    alliance_restore_save_state(saved);
    civs[0] = old_civ0; civs[1] = old_civ1; civ_count = old_count;
    artifacts = render_alpha(PROBE_DIR "/map_alpha_country_53.bmp", "Country", sc) &&
                render_alpha(PROBE_DIR "/map_alpha_alliance_independent_53.bmp", "Alliance independent", si) &&
                render_alpha(PROBE_DIR "/map_alpha_alliance_member_69.bmp", "Alliance member", sm);
    snapshot_ok &= sc.alpha == 136 && si.alpha == 136 && sm.alpha == 176;
    live_ok &= lc.alpha == 136 && li.alpha == 136 && lm.alpha == 176;
    fprintf(summary,
        "case=map_fill_alpha_policy ok=%d country=%d alliance_independent=%d alliance_member=%d snapshot=%d live=%d\n",
        snapshot_ok && live_ok && artifacts, sc.alpha, si.alpha, sm.alpha, snapshot_ok, live_ok);
    free(state); free(saved); free(snapshot);
    return snapshot_ok && live_ok && artifacts;
}

int game_presentation_world_policy_probe(FILE *summary) {
    return case_bottom_row(summary) & case_alpha_policy(summary);
}
