#include "render/panel_debug_controls.h"

#include "core/plague_perf.h"
#include "render/render_common.h"
#include "render/profiling_switches.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"

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

static const char *feature_switch_label(int index) {
    switch (index) {
        case DEBUG_PLAGUE_SWITCH_SYSTEM: return tr("Plague System", "瘟疫系统");
        case DEBUG_PLAGUE_SWITCH_VISUALS: return tr("Plague Map Visuals", "瘟疫地图视觉");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_SIDE_PANEL_DETAIL:
            return tr("Side panel detail draw", "侧栏详情绘制");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_HIGHLIGHT:
            return tr("Highlight", "高亮");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_CITY_OVERLAY:
            return tr("City overlay", "城市覆盖层");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_MAP_LABELS:
            return tr("Map labels", "地图标签");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_STATIC_SCENE:
            return tr("Static scene rebuild", "静态场景重建");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_DIPLOMACY_ANIMATION:
            return tr("Diplomacy animation", "外交动画");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_MAP_LEGEND:
            return tr("Map legend", "地图图例");
        case DEBUG_FEATURE_SWITCH_PROFILE_FIRST + PROFILING_SWITCH_PANEL_CACHE_REBUILD:
            return tr("Panel cache rebuild", "面板缓存重建");
        default:
            return tr("Unknown", "未知");
    }
}

static int feature_switch_enabled(int index) {
    if (index == DEBUG_PLAGUE_SWITCH_SYSTEM) return plague_perf_system_enabled();
    if (index == DEBUG_PLAGUE_SWITCH_VISUALS) return plague_perf_map_visuals_enabled();
    return profiling_switch_enabled(index - DEBUG_FEATURE_SWITCH_PROFILE_FIRST);
}

static void draw_switch_row(HDC hdc, UiCursor *cursor, const char *label, int enabled) {
    RECT row;
    RECT button;
    RECT accent;
    RECT state_text;
    UiClaySemanticStyle tone = ui_clay_semantic_style(enabled ? UI_CLAY_TONE_PEACE : UI_CLAY_TONE_WAR);
    UiClayState state;

    if (cursor->y > cursor->bottom - 28) return;
    row = ui_take_rect(cursor, 26);
    button = (RECT){row.right - 88, row.top + 2, row.right, row.bottom - 2};
    state_text = (RECT){button.left - 42, row.top, button.left - 8, row.bottom};
    state = ui_clay_state_for_rect(button, hover_x, hover_y, 0, 0);
    draw_text_rect(hdc, (RECT){row.left, row.top, state_text.left - 8, row.bottom}, label,
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, state_text, on_off_text(enabled), tone.tag_text,
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    ui_clay_draw_pill(hdc, button, state);
    accent = (RECT){button.left + 8, button.top + 5, button.left + 12, button.bottom - 5};
    fill_rect(hdc, accent, tone.accent);
}

void draw_debug_plague_perf_controls(HDC hdc, UiCursor *cursor) {
    int i;
    ui_section(hdc, cursor, tr("Feature Switches", "功能开关"));
    for (i = 0; i < DEBUG_FEATURE_SWITCH_COUNT; i++) {
        draw_switch_row(hdc, cursor, feature_switch_label(i), feature_switch_enabled(i));
    }
    cursor->y += 6;
}

int debug_panel_plague_perf_switch_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    RECT clip = {client.right - side_panel_w + FORM_X_PAD,
                 TOP_BAR_H + 62 + 32 + 28 + 10,
                 client.right - FORM_X_PAD,
                 client.bottom - 64};
    if (!point_in_rect(clip, mouse_x, mouse_y)) return -1;
    for (i = 0; i < DEBUG_FEATURE_SWITCH_COUNT; i++) {
        if (point_in_rect(plague_switch_button_rect(client, i), mouse_x, mouse_y)) return i;
    }
    return -1;
}
