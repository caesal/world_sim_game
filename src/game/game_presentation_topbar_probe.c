#include "render/render_panel_internal.h"

#include "core/game_state.h"
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

int game_presentation_topbar_probe(FILE *summary) {
    RECT expanded = {0, 0, 1280, 720};
    RECT collapsed = {0, 0, 620, 720};
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_language = ui_language, layout_ok, artifact_ok;

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
    artifact_ok = render_topbar_bmp(PRESENTATION_PROBE_DIR "/topbar_expanded_en.bmp", 0, UI_LANG_EN, 1280);
    artifact_ok &= render_topbar_bmp(PRESENTATION_PROBE_DIR "/topbar_collapsed_en.bmp", 1, UI_LANG_EN, 620);
    artifact_ok &= render_topbar_bmp(PRESENTATION_PROBE_DIR "/topbar_expanded_zh.bmp", 0, UI_LANG_ZH, 1280);
    artifact_ok &= render_topbar_bmp(PRESENTATION_PROBE_DIR "/topbar_collapsed_zh.bmp", 1, UI_LANG_ZH, 620);
    fprintf(summary,
            "case=topbar_reset_layout ok=%d layout=%d artifacts=%d files=topbar_expanded_en.bmp/topbar_collapsed_en.bmp/topbar_expanded_zh.bmp/topbar_collapsed_zh.bmp\n",
            layout_ok && artifact_ok, layout_ok, artifact_ok);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    ui_language = old_language;
    return layout_ok && artifact_ok;
}
