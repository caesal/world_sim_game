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

static void format_percent(char *out, int out_size, int progress_units) {
    progress_units = clamp(progress_units, 0, 100000);
    snprintf(out, out_size, "%d.%d%%", progress_units / 1000, (progress_units / 100) % 10);
}

static void draw_progress_bar(HDC hdc, RECT bar, int progress_units, COLORREF color) {
    RECT fill = bar;
    progress_units = clamp(progress_units, 0, 100000);
    fill.right = fill.left + (bar.right - bar.left) * progress_units / 100000;
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
    const char *stage_name;
    char text[160];
    char pct[32];
    char stage_pct[32];

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

    stage_name = ui_language == UI_LANG_ZH ? worldgen_stage_name_zh(progress.stage) :
                                             worldgen_stage_name_en(progress.stage);
    format_percent(pct, sizeof(pct), progress.overall_progress_units);
    format_percent(stage_pct, sizeof(stage_pct), progress.stage_progress_units);
    draw_text_rect(hdc, title, tr("Generating world", "正在生成世界"),
                   RGB(238, 244, 248), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    snprintf(text, sizeof(text), "%s %s", tr("Overall progress", "总体进度"), pct);
    draw_text_rect(hdc, overall_label, text, RGB(210, 224, 232), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_progress_bar(hdc, overall_bar, progress.overall_progress_units, RGB(103, 164, 196));
    snprintf(text, sizeof(text), "%s: %s %s", tr("Current stage", "当前阶段"), stage_name, stage_pct);
    draw_text_rect(hdc, stage_label, text, RGB(210, 224, 232),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (progress.stage_total > 0) {
        snprintf(text, sizeof(text), "%d / %d", progress.stage_current, progress.stage_total);
        draw_text_rect(hdc, stage_count, text, RGB(176, 192, 204), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    draw_progress_bar(hdc, stage_bar, progress.stage_progress_units, RGB(154, 184, 112));
}
