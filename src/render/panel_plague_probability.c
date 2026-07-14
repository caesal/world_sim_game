#include "render/panel_plague_probability.h"

#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static const char *bucket_label(int bucket) {
    if (bucket == PLAGUE_PROBABILITY_SMALL) {
        return tr("Small plague", "小型瘟疫");
    }
    if (bucket == PLAGUE_PROBABILITY_MEDIUM) {
        return tr("Medium plague", "中型瘟疫");
    }
    if (bucket == PLAGUE_PROBABILITY_LARGE) {
        return tr("Large plague", "大型瘟疫");
    }
    return tr("No plague", "无瘟疫");
}

static int bucket_value(const PlagueProbabilityDistribution *probabilities,
                        int bucket) {
    if (bucket == PLAGUE_PROBABILITY_SMALL) return probabilities->small;
    if (bucket == PLAGUE_PROBABILITY_MEDIUM) return probabilities->medium;
    if (bucket == PLAGUE_PROBABILITY_LARGE) return probabilities->large;
    return probabilities->no_plague;
}

static COLORREF bucket_color(int bucket) {
    if (bucket == PLAGUE_PROBABILITY_SMALL) {
        return ui_theme_color(UI_COLOR_GOOD);
    }
    if (bucket == PLAGUE_PROBABILITY_MEDIUM) {
        return ui_theme_color(UI_COLOR_ACCENT);
    }
    if (bucket == PLAGUE_PROBABILITY_LARGE) {
        return ui_theme_color(UI_COLOR_DANGER);
    }
    return ui_theme_color(UI_COLOR_TEXT_DIM);
}

static void draw_track(HDC hdc, RECT track, int value, COLORREF color,
                       UiClayState clay_state) {
    RECT inner = track;
    RECT fill;
    RECT knob;
    int travel = max(1, track.right - track.left - 1);
    int center = track.left + travel * clamp(value, 0, 100) / 100;
    int center_y = track.top + (track.bottom - track.top) / 2;
    ui_clay_draw_pill_inset(hdc, track, UI_CLAY_STATE_NORMAL);
    InflateRect(&inner, -2, -2);
    if (inner.right > inner.left && inner.bottom > inner.top) {
        fill_rect(hdc, inner, RGB(47, 58, 63));
        fill = inner;
        fill.right = max(fill.left, min(center, fill.right));
        if (fill.right > fill.left) fill_rect(hdc, fill, color);
    }
    knob = (RECT){center - 7, center_y - 7,
                  center + 7, center_y + 7};
    ui_clay_draw_pill(hdc, knob, clay_state);
}

static void draw_control(HDC hdc,
    const UiPlagueProbabilityControlLayout *layout, int bucket, int value,
    int hover, int pressed) {
    char text[16];
    UiClayState state = ui_clay_state_from_flags(hover, pressed, 0, 0);
    snprintf(text, sizeof(text), "%d%%", value);
    draw_text_rect(hdc, layout->label, bucket_label(bucket),
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->value, text,
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    draw_track(hdc, layout->track, value, bucket_color(bucket), state);
}

void plague_panel_probability_draw(HDC hdc,
    const UiPlagueProbabilityLayout *layout,
    const RenderSnapshot *snapshot) {
    PlagueProbabilityDistribution draft;
    int hover = ui_plague_panel_hover_target();
    int apply_enabled;
    int reset_enabled;
    int i;

    if (!layout) return;
    ui_plague_probability_sync(snapshot ? &snapshot->plague_state : NULL);
    ui_plague_probability_get_draft(&draft);
    apply_enabled = ui_plague_probability_apply_enabled();
    reset_enabled = ui_plague_probability_reset_enabled();
    ui_clay_draw_section_header(hdc, layout->title,
        tr("Next Plague Check", "下次瘟疫判定"));
    for (i = 0; i < PLAGUE_PROBABILITY_COUNT; i++) {
        int hit = UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE + i;
        draw_control(hdc, &layout->controls[i], i,
                     bucket_value(&draft, i), hover == hit,
                     hover == hit && ui_plague_probability_pressed(hit));
    }
    draw_text_rect(hdc, layout->total, tr("Total 100%", "合计 100%"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    ui_clay_draw_button(hdc, layout->apply_button, tr("Apply", "应用"),
        ui_clay_state_from_flags(
            hover == UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY,
            hover == UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY &&
                ui_plague_probability_pressed(
                    UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY),
            0, !apply_enabled));
    ui_clay_draw_button(hdc, layout->reset_button,
        tr("Reset defaults", "恢复默认"),
        ui_clay_state_from_flags(
            hover == UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET,
            hover == UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET &&
                ui_plague_probability_pressed(
                    UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET),
            0, !reset_enabled));
    draw_text_rect(hdc, layout->status,
        snapshot && snapshot->plague_state.pending_probabilities_valid ?
            tr("Pending: applies when the current plague ends.",
               "待生效：当前瘟疫结束后应用。") :
            tr("Effective for the next eligible check.",
               "对下一次符合条件的检查生效。"),
        ui_theme_color(UI_COLOR_TEXT_DIM),
        DT_LEFT | DT_TOP | DT_WORDBREAK);
}
