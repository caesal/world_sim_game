#include "game/game_presentation_war_history_visual_contract_probe.h"

#include "render/panel_country_diplomacy_war_history.h"
#include "sim/diplomacy.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uintptr_t recorded_font_handle;
static int recorded_font_height;
static int recorded_font_ascent;

static int read_source(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t used;
    if (!file || size < 2) return 0;
    used = fread(buffer, 1, size - 1, file);
    buffer[used] = '\0';
    if (!feof(file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static int occurrences(const char *text, const char *needle) {
    int count = 0;
    size_t length = strlen(needle);
    while ((text = strstr(text, needle)) != NULL) {
        count++;
        text += length;
    }
    return count;
}

int game_presentation_war_history_visual_contract_source_ok(void) {
    static const char *required[] = {
        "#define WAR_HISTORY_CARD_GAP 6",
        "#define WAR_HISTORY_PRODUCTION_LINE_H 17",
        "#define WAR_HISTORY_WIDE_MIN 500",
        "#define WAR_HISTORY_ROW_PAD 4",
        "#define WAR_HISTORY_SWATCH 10",
        "war_history_card_layout",
        "war_history_card_layout_for_metrics",
        "country_diplomacy_war_history_card_height",
        "War Ended", "战争结束",
        "Recently Ended", "最近结束",
        "Casualties %s", "阵亡 %s",
        "Ended Y%d M%d", "%d年%d月结束 · 持续%d年%d个月",
        "No cession · No reparations", "无割让 · 无赔款",
        "record->beneficiary_uid == record->local.uid",
        "record->beneficiary_uid == record->opponent.uid", "←", "→",
        "GetTextMetrics"
    };
    static const char *forbidden[] = {
        "#define WAR_HISTORY_CARD_H 180",
        "#define WAR_HISTORY_CARD_H 90",
        "#define WAR_HISTORY_TITLE_H",
        "#define WAR_HISTORY_MATCHUP_H",
        "#define WAR_HISTORY_FOOTER_H",
        "#define WAR_HISTORY_CARD_GAP 8",
        "draw_principal_panel",
        "format_metric_value",
        "\\n%dmo", "\\n%d个月",
        "CreateFontW", "CreateFont(", "DeleteObject", "SelectObject",
        "compact_font", "footer_font", "malloc(", "calloc(", "realloc("
    };
    char source[65536];
    char panel_source[65536];
    char cards_source[65536];
    const char *anchor;
    const char *army;
    const char *history;
    const char *active;
    int required_ok = 1;
    int forbidden_ok = 1;
    int i;
    if (!read_source("src/render/panel_country_diplomacy_war_history.c",
                     source, sizeof(source)) ||
        !read_source("src/render/panel_country_diplomacy.c",
                     panel_source, sizeof(panel_source)) ||
        !read_source("src/render/panel_country_diplomacy_cards.c",
                     cards_source, sizeof(cards_source))) return 0;
    for (i = 0; i < (int)(sizeof(required) / sizeof(required[0])); i++) {
        if (!strstr(source, required[i])) required_ok = 0;
    }
    for (i = 0; i < (int)(sizeof(forbidden) / sizeof(forbidden[0])); i++) {
        if (strstr(source, forbidden[i])) forbidden_ok = 0;
    }
    anchor = strstr(panel_source, "void draw_country_diplomacy_tab");
    army = anchor ? strstr(anchor, "draw_army_pool(") : NULL;
    history = anchor ? strstr(
        anchor, "draw_country_diplomacy_war_history(") : NULL;
    active = anchor ? strstr(anchor, "draw_diplomacy_group(") : NULL;
    return required_ok && forbidden_ok && army && history && active &&
           army < history && history < active &&
           occurrences(source, "ui_clay_draw_card(hdc, card") == 1 &&
           !strstr(cards_source, "CreateFont") &&
           !strstr(cards_source, "SelectObject") &&
           !strstr(cards_source, "DeleteObject") &&
           strstr(source, "DT_END_ELLIPSIS") &&
           !strstr(source, "SetTextCharacterExtra") &&
           !strstr(source, "Updating...") && !strstr(source, "更新中") &&
           !strstr(source, "war_history_copy_for_civ") &&
           !strstr(source, "war_history_get") &&
           !strstr(source, "state_lock") &&
           !strstr(source, "simulation_");
}

static int rect_height(RECT rect) {
    return rect.bottom - rect.top;
}

static int rect_width(RECT rect) {
    return rect.right - rect.left;
}

static int inherited_font_contract_ok(void) {
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    HFONT font = NULL;
    HGDIOBJ old_font = NULL;
    HGDIOBJ incoming;
    TEXTMETRICW before;
    TEXTMETRICW after;
    SnapshotCiv civ = {0};
    SnapshotWarHistoryRecord *record = &civ.war_history.records[0];
    UiCursor cursor;
    DWORD gdi_before;
    DWORD gdi_after;
    int saved_width = side_panel_w;
    int saved_language = ui_language;
    int fits;
    int ok = 0;
    if (!hdc) goto cleanup;
    bitmap = CreateCompatibleBitmap(hdc, 500, 500);
    font = CreateFontW(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!bitmap || !font) goto cleanup;
    old_bitmap = SelectObject(hdc, bitmap);
    old_font = SelectObject(hdc, font);
    incoming = GetCurrentObject(hdc, OBJ_FONT);
    if (!incoming || !GetTextMetricsW(hdc, &before)) goto cleanup;
    recorded_font_handle = (uintptr_t)incoming;
    recorded_font_height = before.tmHeight;
    recorded_font_ascent = before.tmAscent;

    civ.uid = 77;
    civ.war_history.owner_uid = 77;
    civ.war_history.count = 1;
    record->local.uid = 77;
    record->opponent.uid = 88;
    record->local.color = RGB(190, 58, 132);
    record->opponent.color = RGB(58, 68, 198);
    strcpy(record->local.name_en, "Local Commonwealth");
    strcpy(record->local.name_zh, "本地联邦");
    strcpy(record->opponent.name_en, "Opponent Federation");
    strcpy(record->opponent.name_zh, "对方联邦");
    record->result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    record->local_casualties = 26340;
    record->opponent_casualties = 41780;
    record->end_year = 143;
    record->end_month = 6;
    record->duration_months = 28;
    side_panel_w = 500;
    ui_language = UI_LANG_EN;
    cursor = ui_cursor(8, 0, 476, 500);
    gdi_before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    draw_country_diplomacy_war_history(hdc, &cursor, &civ);
    fits = country_diplomacy_war_history_record_text_fits(
        hdc, 500, 476, record);
    GdiFlush();
    gdi_after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    ok = fits && cursor.y == 145 &&
         GetCurrentObject(hdc, OBJ_FONT) == incoming &&
         GetTextMetricsW(hdc, &after) &&
         after.tmHeight == before.tmHeight &&
         after.tmAscent == before.tmAscent &&
         gdi_after == gdi_before;

cleanup:
    side_panel_w = saved_width;
    ui_language = saved_language;
    if (old_font && old_font != HGDI_ERROR) SelectObject(hdc, old_font);
    if (old_bitmap && old_bitmap != HGDI_ERROR) SelectObject(hdc, old_bitmap);
    if (font) DeleteObject(font);
    if (bitmap) DeleteObject(bitmap);
    if (hdc) DeleteDC(hdc);
    return ok;
}

static int geometry_ok(void) {
    static const int widths[] = {340, 460, 500, 720};
    int width_index;
    for (width_index = 0; width_index < 4; width_index++) {
        int card_height = country_diplomacy_war_history_card_height(
            widths[width_index], 17);
        RECT card = {8, 100, widths[width_index] - 8,
                     100 + card_height};
        WarHistoryCardLayout layout;
        int card_center = (card.left + card.right) / 2;
        int result_center;
        war_history_card_layout_for_metrics(
            card, widths[width_index], 17, &layout);
        result_center = (layout.result.left + layout.result.right) / 2;
        if (layout.card.left != card.left || layout.card.bottom != card.bottom ||
            rect_height(layout.card) != 114 ||
            rect_height(layout.title) != 21 ||
            layout.matchup.top != layout.title.bottom ||
            rect_height(layout.matchup) != 55 ||
            layout.footer.top != layout.matchup.bottom ||
            rect_height(layout.footer) != 38 ||
            layout.footer.bottom != card.bottom ||
            layout.line_height != 17 ||
            layout.wide != (widths[width_index] >= 500) ||
            layout.footer_rows != (widths[width_index] >= 500 ? 1 : 2) ||
            rect_width(layout.local_swatch) != 10 ||
            rect_height(layout.local_swatch) != 10 ||
            rect_width(layout.opponent_swatch) != 10 ||
            rect_height(layout.opponent_swatch) != 10 ||
            abs(result_center - card_center) > 1 ||
            layout.local_name.right > layout.result.left ||
            layout.opponent_name.left < layout.result.right ||
            layout.local_casualties.bottom != layout.matchup.bottom ||
            layout.opponent_casualties.bottom != layout.matchup.bottom) return 0;
        if (layout.wide) {
            if (rect_height(layout.local_name) != 36 ||
                layout.settlement.right > layout.date.left ||
                layout.settlement.top != layout.date.top ||
                layout.settlement.bottom != layout.date.bottom) return 0;
        } else {
            if (rect_height(layout.local_name) != 19 ||
                layout.settlement.left != layout.date.left ||
                layout.settlement.right != layout.date.right ||
                layout.settlement.bottom != layout.date.top) return 0;
        }
    }
    {
        SnapshotCiv civ = {0};
        int saved_width = side_panel_w;
        int heights_ok;
        civ.uid = 77;
        civ.war_history.owner_uid = 77;
        side_panel_w = 500;
        heights_ok = country_diplomacy_war_history_height(&civ) == 0;
        civ.war_history.count = 1;
        heights_ok &= country_diplomacy_war_history_height(&civ) == 145;
        civ.war_history.count = 2;
        heights_ok &= country_diplomacy_war_history_height(&civ) == 265;
        civ.war_history.count = 3;
        heights_ok &= country_diplomacy_war_history_height(&civ) == 385;
        side_panel_w = saved_width;
        if (!heights_ok) return 0;
    }
    return 1;
}

int game_presentation_war_history_visual_contract_probe(FILE *summary) {
    int source_ok = game_presentation_war_history_visual_contract_source_ok();
    int layout_ok = geometry_ok();
    int font_ok = inherited_font_contract_ok();
    fprintf(summary,
            "case=war_history_visual_source_contract ok=%d required_and_forbidden=%d geometry=%d inherited_font_metrics=%d font_handle=0x%llx tm_height=%d tm_ascent=%d widths=340/460/500/720 line_height=17 fixed_cards=114 gap=6 swatch=10 title=1 footer_rows=2/2/1/1 heights=0/145/265/385\n",
            source_ok && layout_ok && font_ok, source_ok, layout_ok,
            font_ok, (unsigned long long)recorded_font_handle,
            recorded_font_height, recorded_font_ascent);
    return source_ok && layout_ok && font_ok;
}
