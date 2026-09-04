#include "render/render_panel_internal.h"
#include "game/game_presentation_static_camera_resources.h"
#include "game/game_presentation_static_physical_artifacts.h"

#include "core/game_state.h"
#include "ui/ui_map_display.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

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

static int rect_intersects(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static int rect_inside(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom &&
           inner.right > inner.left && inner.bottom > inner.top;
}

static int topbar_layout_ok(RECT client) {
    RECT reset = get_reset_view_button_rect(client);
    RECT language = get_language_button_rect(client);
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    int i;
    if (!rect_inside(top, reset) || !rect_inside(top, language) ||
        rect_intersects(reset, language)) return 0;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        RECT mode = get_mode_button_rect(client, i);
        if (!rect_inside(top, mode) || rect_intersects(mode, reset) ||
            rect_intersects(mode, language)) return 0;
    }
    return 1;
}

static int mode_hit_index(RECT client, int x, int y) {
    int hit = -1;
    int count = 0;
    int i;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        if (point_in_rect_local(get_mode_button_rect(client, i), x, y)) {
            hit = i;
            count++;
        }
    }
    return count == 1 ? hit : -1;
}

static int required_topbar_layout_ok(FILE *summary, HDC hdc, RECT client,
                                     int language, int panel_width,
                                     int collapsed, int *labels_passed,
                                     int *hits_passed) {
    const int horizontal_padding = 1;
    int expected_lane_width = collapsed ? MIN_SIDE_PANEL_W - 28 :
                              panel_width - 28;
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT year = {client.right / 2 - 112, 9,
                 client.right / 2 + 112, 50};
    RECT reset = get_reset_view_button_rect(client);
    RECT language_button = get_language_button_rect(client);
    RECT version;
    SIZE version_size = {0};
    int ok = 1;
    int label_count = 0;
    int hit_count = 0;
    int minimum_slack = client.right - client.left;
    int i;

    measure_text_utf8(hdc, WORLD_SIM_VERSION_LABEL, &version_size);
    version = (RECT){18, 20, 18 + version_size.cx, 20 + version_size.cy};
    ok &= rect_inside(top, year) && rect_inside(top, reset) &&
          rect_inside(top, language_button) && rect_inside(top, version) &&
          !rect_intersects(version, year) &&
          !rect_intersects(version, reset) &&
          !rect_intersects(version, language_button) &&
          !rect_intersects(year, reset) &&
          !rect_intersects(year, language_button) &&
          !rect_intersects(reset, language_button);
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        RECT mode = get_mode_button_rect(client, i);
        SIZE text_size = {0};
        int button_width = mode.right - mode.left;
        int center_x = mode.left + button_width / 2;
        int center_y = mode.top + (mode.bottom - mode.top) / 2;
        int label_ok;
        int geometry_ok;
        int hit_ok;
        int j;

        measure_text_utf8(hdc, ui_map_display_label(i, language), &text_size);
        label_ok = text_size.cx > 0 && text_size.cx < button_width &&
                   text_size.cx + horizontal_padding * 2 <= button_width;
        geometry_ok = rect_inside(top, mode) &&
                      !rect_intersects(mode, version) &&
                      !rect_intersects(mode, year) &&
                      !rect_intersects(mode, reset) &&
                      !rect_intersects(mode, language_button);
        for (j = 0; j < i; j++) {
            geometry_ok &= !rect_intersects(
                mode, get_mode_button_rect(client, j));
        }
        if (i > 0) {
            RECT previous = get_mode_button_rect(client, i - 1);
            geometry_ok &= mode.left - previous.right == 6;
        }
        hit_ok = mode_hit_index(client, center_x, center_y) == i;
        if (label_ok) label_count++;
        if (hit_ok) hit_count++;
        if (button_width - text_size.cx < minimum_slack) {
            minimum_slack = button_width - text_size.cx;
        }
        fprintf(summary,
                "case=topbar_full_label_fit ok=%d language=%s panel_width=%d collapsed=%d mode_index=%d label=%s measured_width=%ld button_width=%d horizontal_padding_each_side=%d geometry=%d hit_target=%d\n",
                label_ok && geometry_ok && hit_ok,
                language == UI_LANG_ZH ? "zh" : "en", panel_width,
                collapsed, i, ui_map_display_label(i, language),
                (long)text_size.cx, button_width, horizontal_padding,
                geometry_ok, hit_ok);
        ok &= label_ok && geometry_ok && hit_ok;
    }
    {
        RECT first = get_mode_button_rect(client, 0);
        RECT last = get_mode_button_rect(client, MAP_DISPLAY_MODE_COUNT - 1);
        int lane_width = last.right - first.left;
        int lane_ok = lane_width == expected_lane_width;
        ok &= lane_ok;
        fprintf(summary,
                "case=topbar_full_label_case ok=%d language=%s panel_width=%d collapsed=%d lane_width=%d expected_lane_width=%d lane_ok=%d labels=%d/6 hit_targets=%d/6 min_horizontal_slack=%d\n",
                ok, language == UI_LANG_ZH ? "zh" : "en", panel_width,
                collapsed, lane_width, expected_lane_width, lane_ok,
                label_count, hit_count, minimum_slack);
    }
    if (labels_passed) *labels_passed = label_count;
    if (hits_passed) *hits_passed = hit_count;
    return ok;
}

static int render_topbar_bmp(const char *path, int collapsed, int language, int width) {
    const int height = 92;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_language = ui_language, ok;

    side_panel_collapsed = collapsed;
    side_panel_w = 380;
    ui_language = language;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, client, RGB(18, 24, 28));
    draw_top_bar(hdc, client);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    ui_language = old_language;
    return ok;
}

typedef struct {
    const char *artifact;
    int language;
    int panel_width;
    int collapsed;
} RequiredTopbarCase;

static int render_required_topbar_bmp(FILE *summary,
                                      const RequiredTopbarCase *test,
                                      int *labels_passed,
                                      int *hits_passed) {
    const int width = 2560;
    const int height = 1369;
    HDC screen = GetDC(NULL);
    HDC hdc = screen ? CreateCompatibleDC(screen) : NULL;
    BITMAPINFO info;
    HBITMAP bitmap = NULL;
    HBITMAP old_bitmap = NULL;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    int old_collapsed = side_panel_collapsed;
    int old_side = side_panel_w;
    int old_expanded_side = side_panel_expanded_w;
    int old_language = ui_language;
    int old_display_mode = display_mode;
    int old_hover_x = hover_x;
    int old_hover_y = hover_y;
    int layout_ok = 0;
    int artifact_ok = 0;

    if (!test || !screen || !hdc) goto done;
    side_panel_collapsed = test->collapsed;
    side_panel_w = test->panel_width;
    side_panel_expanded_w = test->panel_width;
    ui_language = test->language;
    display_mode = DISPLAY_GEOGRAPHY;
    hover_x = -1;
    hover_y = -1;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) goto done;
    old_bitmap = (HBITMAP)SelectObject(hdc, bitmap);
    layout_ok = required_topbar_layout_ok(
        summary, hdc, client, test->language, test->panel_width,
        test->collapsed, labels_passed, hits_passed);
    {
        RECT hover = get_mode_button_rect(client, MAP_DISPLAY_MODE_COUNT - 1);
        hover_x = hover.left + (hover.right - hover.left) / 2;
        hover_y = hover.top + (hover.bottom - hover.top) / 2;
    }
    fill_rect(hdc, client, RGB(18, 24, 28));
    draw_top_bar(hdc, client);
    artifact_ok = write_bmp(static_physical_probe_artifact_path(test->artifact),
                            &info, bits, width, height);

done:
    if (hdc && old_bitmap) SelectObject(hdc, old_bitmap);
    if (bitmap) DeleteObject(bitmap);
    if (hdc) DeleteDC(hdc);
    if (screen) ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    side_panel_expanded_w = old_expanded_side;
    ui_language = old_language;
    display_mode = old_display_mode;
    hover_x = old_hover_x;
    hover_y = old_hover_y;
    fprintf(summary,
            "case=topbar_full_label_artifact ok=%d language=%s panel_width=%d collapsed=%d width=%d height=%d file=%s layout=%d wrote=%d selected_geography=1\n",
            layout_ok && artifact_ok,
            test && test->language == UI_LANG_ZH ? "zh" : "en",
            test ? test->panel_width : 0, test ? test->collapsed : 0,
            width, height, test ? test->artifact : "invalid", layout_ok,
            artifact_ok);
    return layout_ok && artifact_ok;
}

int game_presentation_topbar_probe(FILE *summary) {
    static const RequiredTopbarCase required_cases[] = {
        {"topbar_fullfit_en_500_expanded.bmp", UI_LANG_EN, 500, 0},
        {"topbar_fullfit_en_500_collapsed.bmp", UI_LANG_EN, 500, 1},
        {"topbar_fullfit_en_720_expanded.bmp", UI_LANG_EN, 720, 0},
        {"topbar_fullfit_en_720_collapsed.bmp", UI_LANG_EN, 720, 1},
        {"topbar_fullfit_zh_500_expanded.bmp", UI_LANG_ZH, 500, 0},
        {"topbar_fullfit_zh_500_collapsed.bmp", UI_LANG_ZH, 500, 1},
        {"topbar_fullfit_zh_720_expanded.bmp", UI_LANG_ZH, 720, 0},
        {"topbar_fullfit_zh_720_collapsed.bmp", UI_LANG_ZH, 720, 1}
    };
    RECT expanded = {0, 0, 1280, 720};
    RECT collapsed = {0, 0, 620, 720};
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_language = ui_language, layout_ok, artifact_ok;
    StaticCameraResources resources_before;
    StaticCameraResources resources_after;
    DWORD user_before;
    DWORD user_after;
    int resources_ok;
    int required_cases_passed = 0;
    int required_labels_passed = 0;
    int required_hits_passed = 0;
    size_t i;

    side_panel_w = 380;
    side_panel_collapsed = 0;
    ui_language = UI_LANG_EN;
    layout_ok = topbar_layout_ok(expanded);
    ui_language = UI_LANG_ZH;
    layout_ok &= topbar_layout_ok(expanded);
    side_panel_collapsed = 1;
    ui_language = UI_LANG_EN;
    layout_ok &= topbar_layout_ok(collapsed);
    ui_language = UI_LANG_ZH;
    layout_ok &= topbar_layout_ok(collapsed);
    artifact_ok = render_topbar_bmp(static_physical_probe_artifact_path("topbar_expanded_en.bmp"), 0, UI_LANG_EN, 1280);
    artifact_ok &= render_topbar_bmp(static_physical_probe_artifact_path("topbar_collapsed_en.bmp"), 1, UI_LANG_EN, 620);
    artifact_ok &= render_topbar_bmp(static_physical_probe_artifact_path("topbar_expanded_zh.bmp"), 0, UI_LANG_ZH, 1280);
    artifact_ok &= render_topbar_bmp(static_physical_probe_artifact_path("topbar_collapsed_zh.bmp"), 1, UI_LANG_ZH, 620);
    resources_before = static_camera_resources_capture();
    user_before = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (i = 0; i < sizeof(required_cases) / sizeof(required_cases[0]); i++) {
        int labels_passed = 0;
        int hits_passed = 0;
        int case_ok = render_required_topbar_bmp(
            summary, &required_cases[i], &labels_passed, &hits_passed);
        required_cases_passed += case_ok;
        required_labels_passed += labels_passed;
        required_hits_passed += hits_passed;
        artifact_ok &= case_ok;
    }
    resources_after = static_camera_resources_capture();
    user_after = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    resources_ok = resources_before.valid && resources_after.valid &&
                   resources_before.gdi_objects == resources_after.gdi_objects &&
                   user_before == user_after &&
                   resources_after.private_bytes <= resources_before.private_bytes;
    fprintf(summary,
            "case=topbar_full_label_resources ok=%d gdi_before=%lu gdi_after=%lu user_before=%lu user_after=%lu private_before=%llu private_after=%llu working_set_before=%llu working_set_after=%llu working_set_diagnostic_only=1\n",
            resources_ok, (unsigned long)resources_before.gdi_objects,
            (unsigned long)resources_after.gdi_objects,
            (unsigned long)user_before, (unsigned long)user_after,
            resources_before.private_bytes, resources_after.private_bytes,
            resources_before.working_set, resources_after.working_set);
    fprintf(summary,
            "case=topbar_full_label_matrix ok=%d cases=%d/8 labels=%d/48 hit_targets=%d/48 artifacts=%d/8 client=2560x1369 constrained_620_preserved=1\n",
            required_cases_passed == 8 && required_labels_passed == 48 &&
            required_hits_passed == 48,
            required_cases_passed, required_labels_passed,
            required_hits_passed, required_cases_passed);
    fprintf(summary,
            "case=topbar_reset_layout ok=%d layout=%d artifacts=%d resources=%d constrained_files=topbar_expanded_en.bmp/topbar_collapsed_en.bmp/topbar_expanded_zh.bmp/topbar_collapsed_zh.bmp required_fullfit_artifacts=8\n",
            layout_ok && artifact_ok && resources_ok, layout_ok, artifact_ok,
            resources_ok);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    ui_language = old_language;
    return layout_ok && artifact_ok && resources_ok;
}
