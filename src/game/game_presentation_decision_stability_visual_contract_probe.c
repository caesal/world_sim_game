#include "game/game_presentation_decision_stability_visual_contract_probe.h"

#include "render/panel_country_decision.h"

#include <stdio.h>
#include <string.h>

static int read_source(char *buffer, size_t size) {
    FILE *file = fopen("src/render/panel_country_decision_extra.c", "rb");
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

int game_presentation_decision_stability_visual_contract_source_ok(void) {
    static const char *required[] = {
        "#define STABILITY_STATUS_H 30",
        "#define STABILITY_FACTOR_ROW_H 50",
        "#define STABILITY_TOTAL_H 36",
        "country_decision_stability_layout",
        "draw_stability_factor_cell",
        "ui_progress_bar(hdc, bar",
        "Stability Mode: %s    Duration: %s",
        "稳定模式：%s    持续：%s",
        "Monthly Total", "本月合计"
    };
    static const char *forbidden[] = {
        "draw_stability_factor_card",
        "draw_stability_final_intent",
        "tr(\"Exit\"",
        "tr(\"Final Intent\"",
        "const int card_h = 84",
        "ui_clay_draw_progress_bar",
        "CreateFontW",
        "compact_font",
        "SelectObject",
        "DeleteObject",
        "base_detail",
        "max(Disorder"
    };
    char source[65536];
    int required_ok = 1;
    int forbidden_ok = 1;
    int i;
    if (!read_source(source, sizeof(source))) return 0;
    for (i = 0; i < (int)(sizeof(required) / sizeof(required[0])); i++) {
        if (!strstr(source, required[i])) required_ok = 0;
    }
    for (i = 0; i < (int)(sizeof(forbidden) / sizeof(forbidden[0])); i++) {
        if (strstr(source, forbidden[i])) forbidden_ok = 0;
    }
    return required_ok && forbidden_ok;
}

static int rect_height(RECT rect) {
    return rect.bottom - rect.top;
}

static int rect_width(RECT rect) {
    return rect.right - rect.left;
}

static int text_fits(HDC hdc, RECT rect, const char *text) {
    WCHAR wide[128];
    RECT measured = {0, 0, rect_width(rect), 0};
    int length = MultiByteToWideChar(
        CP_UTF8, 0, text, -1, wide,
        (int)(sizeof(wide) / sizeof(wide[0])));
    if (length <= 0) return 0;
    DrawTextW(hdc, wide, -1, &measured,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return measured.right <= rect_width(rect) &&
           measured.bottom <= rect_height(rect);
}

static int inherited_font_and_flat_progress_ok(void) {
    BITMAPINFO info = {0};
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    HFONT font = NULL;
    HGDIOBJ old_font = NULL;
    HGDIOBJ incoming;
    TEXTMETRICW before;
    TEXTMETRICW after;
    DecisionSnapshot snap = {0};
    UiCursor cursor;
    void *bits = NULL;
    int saved_language = ui_language;
    int ok = 0;
    const int width = 500;
    const int left = 10;
    const int right = width - 10;
    const int boundary = left + (right - left) / 2;
    if (!hdc) goto cleanup;
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -700;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    font = CreateFontW(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!bitmap || !font || !bits) goto cleanup;
    old_bitmap = SelectObject(hdc, bitmap);
    old_font = SelectObject(hdc, font);
    incoming = GetCurrentObject(hdc, OBJ_FONT);
    if (!incoming || !GetTextMetricsW(hdc, &before)) goto cleanup;
    ui_language = UI_LANG_EN;

    snap.stability_weight = 0;
    cursor = ui_cursor(8, 42, width - 16, 700);
    draw_country_decision_stability_tab(hdc, &cursor, &snap);
    if (GetCurrentObject(hdc, OBJ_FONT) != incoming ||
        !GetTextMetricsW(hdc, &after) || after.tmHeight != before.tmHeight ||
        GetPixel(hdc, left, 73) != RGB(30, 35, 38) ||
        GetPixel(hdc, right - 1, 90) != RGB(30, 35, 38)) goto cleanup;

    snap.stability_weight = 50;
    cursor = ui_cursor(8, 42, width - 16, 700);
    draw_country_decision_stability_tab(hdc, &cursor, &snap);
    if (GetPixel(hdc, left, 73) != RGB(92, 130, 162) ||
        GetPixel(hdc, boundary - 1, 90) != RGB(92, 130, 162) ||
        GetPixel(hdc, boundary, 90) != RGB(30, 35, 38) ||
        GetPixel(hdc, right - 1, 73) != RGB(30, 35, 38)) goto cleanup;

    snap.stability_weight = 100;
    cursor = ui_cursor(8, 42, width - 16, 700);
    draw_country_decision_stability_tab(hdc, &cursor, &snap);
    if (GetPixel(hdc, left, 73) != RGB(92, 130, 162) ||
        GetPixel(hdc, right - 1, 90) != RGB(92, 130, 162) ||
        GetCurrentObject(hdc, OBJ_FONT) != incoming) goto cleanup;
    ok = 1;

cleanup:
    ui_language = saved_language;
    if (old_font && old_font != HGDI_ERROR) SelectObject(hdc, old_font);
    if (old_bitmap && old_bitmap != HGDI_ERROR) SelectObject(hdc, old_bitmap);
    if (font) DeleteObject(font);
    if (bitmap) DeleteObject(bitmap);
    if (hdc) DeleteDC(hdc);
    return ok;
}

static int supported_text_fit_ok(void) {
    static const char *labels[2][6] = {
        {"Base Pressure", "War Status", "Territory Fragmentation",
         "Capital Connectivity", "Vassal Governance", "High Disorder"},
        {"基础压力", "战争状态", "领土断裂", "首都连通", "附庸治理", "高混乱"}
    };
    static const int widths[2] = {500, 720};
    HDC hdc = CreateCompatibleDC(NULL);
    HFONT font = NULL;
    HGDIOBJ old_font = NULL;
    int ok = 1;
    int w;
    int language;
    int i;
    if (!hdc) return 0;
    font = CreateFontW(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    if (!font) {
        DeleteDC(hdc);
        return 0;
    }
    old_font = SelectObject(hdc, font);
    for (w = 0; w < 2; w++) {
        CountryDecisionStabilityLayout layout;
        country_decision_stability_layout(8, 42, widths[w] - 16, &layout);
        for (language = 0; language < 2; language++) {
            for (i = 0; i < 6; i++) {
                if (!text_fits(hdc, layout.factor_labels[i],
                               labels[language][i])) ok = 0;
            }
        }
    }
    if (old_font && old_font != HGDI_ERROR) SelectObject(hdc, old_font);
    DeleteObject(font);
    DeleteDC(hdc);
    return ok;
}

static int geometry_ok(void) {
    static const int widths[] = {340, 460, 500, 720};
    int width_index;
    for (width_index = 0; width_index < 4; width_index++) {
        CountryDecisionStabilityLayout layout;
        int i;
        country_decision_stability_layout(
            8, 42, widths[width_index] - 16, &layout);
        if (layout.intent.top != 42 || rect_height(layout.intent) != 62 ||
            layout.status.top != layout.intent.bottom ||
            rect_height(layout.status) != 30 ||
            layout.composition.top != layout.status.bottom ||
            rect_height(layout.composition) != 31 ||
            layout.factors[0].top != layout.composition.bottom ||
            layout.factors[0].right != layout.factors[1].left ||
            layout.factors[0].left != 8 ||
            layout.factors[1].right != widths[width_index] - 8 ||
            layout.total.top != layout.factors[4].bottom ||
            rect_height(layout.total) != 36 || layout.bottom != 351) return 0;
        for (i = 0; i < 6; i++) {
            if (rect_height(layout.factors[i]) != 50 ||
                rect_width(layout.factor_accents[i]) != 3 ||
                rect_height(layout.factor_accents[i]) != 50 ||
                layout.factor_labels[i].right >
                    layout.factor_values[i].left ||
                layout.factor_labels[i].left <
                    layout.factor_accents[i].right ||
                layout.factor_labels[i].top != layout.factors[i].top + 2 ||
                layout.factor_labels[i].bottom !=
                    layout.factors[i].bottom - 2 ||
                layout.factor_values[i].top != layout.factors[i].top + 2 ||
                layout.factor_values[i].bottom !=
                    layout.factors[i].bottom - 2) return 0;
            if (i >= 2 && layout.factors[i - 2].bottom !=
                          layout.factors[i].top) return 0;
        }
        if (layout.total_label.right > layout.total_value.left) return 0;
    }
    return 1;
}

int game_presentation_decision_stability_visual_contract_probe(FILE *summary) {
    int source_ok = game_presentation_decision_stability_visual_contract_source_ok();
    int layout_ok = geometry_ok();
    int font_progress_ok = inherited_font_and_flat_progress_ok();
    int text_fit_ok = supported_text_fit_ok();
    fprintf(summary,
            "case=stability_visual_source_contract ok=%d required_and_forbidden=%d geometry=%d inherited_font_flat_progress=%d supported_text_fit=%d widths=340/460/500/720 fixed=62/30/31/50x3/36 flat_cells=1 no_exit=1 no_base_detail=1 monthly_total=1\n",
            source_ok && layout_ok && font_progress_ok && text_fit_ok,
            source_ok, layout_ok, font_progress_ok, text_fit_ok);
    return source_ok && layout_ok && font_progress_ok && text_fit_ok;
}
