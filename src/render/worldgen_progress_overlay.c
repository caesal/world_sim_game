#include "worldgen_progress_overlay.h"

#include "core/game_types.h"
#include "core/worldgen_progress.h"
#include "render/render_common.h"
#include "ui/ui_layout.h"

static void draw_overlay_frame(HDC hdc, RECT rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    HPEN old_pen = SelectObject(hdc, pen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
}

static void draw_progress_bar(HDC hdc, RECT bar, int percent, COLORREF color) {
    RECT fill = bar;
    percent = clamp(percent, 0, 100);
    fill.right = fill.left + (bar.right - bar.left) * percent / 100;
    fill_rect(hdc, bar, RGB(42, 50, 58));
    if (fill.right > fill.left) fill_rect(hdc, fill, color);
    draw_overlay_frame(hdc, bar, RGB(84, 98, 108));
}

void draw_worldgen_progress_overlay(HDC hdc, RECT client) {
    WorldGenProgress progress;
    RECT viewport;
    RECT box;
    RECT title;
    RECT overall_label;
    RECT overall_bar;
    RECT stage_label;
    RECT stage_count;
    RECT stage_bar;
    int width = 460;
    int height = 178;
    const char *title_text;
    const char *stage_name;
    char text[160];

    worldgen_progress_get(&progress);
    if (!progress.active) return;
    viewport = get_map_viewport_rect(client);
    if (viewport.right - viewport.left < width + 32) width = viewport.right - viewport.left - 32;
    if (width < 260) width = 260;
    box.left = viewport.left + (viewport.right - viewport.left - width) / 2;
    box.top = viewport.top + (viewport.bottom - viewport.top - height) / 2;
    box.right = box.left + width;
    box.bottom = box.top + height;

    fill_rect_alpha(hdc, box, RGB(18, 23, 28), 232);
    draw_overlay_frame(hdc, box, RGB(82, 96, 108));
    title = (RECT){box.left + 18, box.top + 14, box.right - 18, box.top + 42};
    overall_label = (RECT){box.left + 18, title.bottom + 6, box.right - 18, title.bottom + 28};
    overall_bar = (RECT){box.left + 18, overall_label.bottom + 3, box.right - 18, overall_label.bottom + 19};
    stage_label = (RECT){box.left + 18, overall_bar.bottom + 12, box.right - 18, overall_bar.bottom + 34};
    stage_count = (RECT){box.left + 18, stage_label.bottom + 2, box.right - 18, stage_label.bottom + 22};
    stage_bar = (RECT){box.left + 18, box.bottom - 34, box.right - 18, box.bottom - 18};

    title_text = ui_language == UI_LANG_ZH ? "正在生成世界" : "Generating world";
    stage_name = ui_language == UI_LANG_ZH ? worldgen_stage_name_zh(progress.stage) :
                                             worldgen_stage_name_en(progress.stage);
    draw_text_rect(hdc, title, title_text, RGB(238, 244, 248), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    snprintf(text, sizeof(text), "%s %d%%", ui_language == UI_LANG_ZH ? "总体进度" : "Overall progress",
             progress.percent);
    draw_text_rect(hdc, overall_label, text, RGB(210, 224, 232), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_progress_bar(hdc, overall_bar, progress.percent, RGB(103, 164, 196));
    snprintf(text, sizeof(text), "%s: %s %d%%", ui_language == UI_LANG_ZH ? "当前阶段" : "Current stage",
             stage_name, progress.stage_percent);
    draw_text_rect(hdc, stage_label, text, RGB(210, 224, 232), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (progress.stage_total > 0) {
        snprintf(text, sizeof(text), "%d / %d", progress.stage_current, progress.stage_total);
        draw_text_rect(hdc, stage_count, text, RGB(176, 192, 204), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    draw_progress_bar(hdc, stage_bar, progress.stage_percent, RGB(154, 184, 112));
}
