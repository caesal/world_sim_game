#include "render/panel_debug_controls.h"

#include "core/plague_perf.h"
#include "render/render_common.h"
#include "ui/ui_theme.h"

static RECT plague_switch_button_rect(RECT client, int index) {
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2;
    int y = TOP_BAR_H + 62 + 32 + 28 + 10;
    y -= debug_system_scroll_offset;
    y += 31 + 21 + 21 + 31 + 26 * index;
    return (RECT){x + width - 88, y + 2, x + width, y + 24};
}

static const char *on_off_text(int enabled) {
    return enabled ? tr("ON", "开") : tr("OFF", "关");
}

static void draw_switch_row(HDC hdc, UiCursor *cursor, const char *label, int enabled) {
    RECT row;
    RECT button;

    if (cursor->y > cursor->bottom - 28) return;
    row = ui_take_rect(cursor, 26);
    button = (RECT){row.right - 88, row.top + 2, row.right, row.bottom - 2};
    draw_text_rect(hdc, (RECT){row.left, row.top, button.left - 8, row.bottom}, label,
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, button, enabled ? RGB(72, 102, 72) : RGB(92, 66, 62));
    draw_center_text(hdc, button, on_off_text(enabled), ui_theme_color(UI_COLOR_TEXT));
}

void draw_debug_plague_perf_controls(HDC hdc, UiCursor *cursor) {
    ui_section(hdc, cursor, tr("Plague Performance", "瘟疫性能"));
    draw_switch_row(hdc, cursor, tr("Plague System", "瘟疫系统"),
                    plague_perf_system_enabled());
    draw_switch_row(hdc, cursor, tr("Plague Map Visuals", "瘟疫地图视觉"),
                    plague_perf_map_visuals_enabled());
    cursor->y += 6;
}

int debug_panel_plague_perf_switch_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    RECT clip = {client.right - side_panel_w + FORM_X_PAD,
                 TOP_BAR_H + 62 + 32 + 28 + 10,
                 client.right - FORM_X_PAD,
                 client.bottom - 64};
    if (!point_in_rect(clip, mouse_x, mouse_y)) return -1;
    for (i = 0; i < DEBUG_PLAGUE_SWITCH_COUNT; i++) {
        if (point_in_rect(plague_switch_button_rect(client, i), mouse_x, mouse_y)) return i;
    }
    return -1;
}
