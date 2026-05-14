#include "render/panel_country_actions.h"

#include "render/render_common.h"
#include "sim/civilization_slots.h"
#include "sim/collapse.h"
#include "sim/simulation.h"
#include "sim/vassal.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static RECT last_civil_unrest_button;
static int last_civil_unrest_enabled;
static RECT last_vassal_buttons[MAX_CIVS];
static int last_vassal_ids[MAX_CIVS];
static int last_vassal_count;

static const char *collapse_block_reason_ui(int civ_id) {
    static char buffers[4][EVENT_LOG_LEN];
    static int index;
    char *buffer = buffers[index++ % 4];
    CollapseBlockReason reason = collapse_block_reason(civ_id);

    switch (reason) {
        case COLLAPSE_BLOCK_NONE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Ready.", "就绪。"));
            break;
        case COLLAPSE_BLOCK_NOT_ALIVE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Country is invalid or has fallen.", "国家无效或已经灭亡。"));
            break;
        case COLLAPSE_BLOCK_MAX_CIVS:
            snprintf(buffer, EVENT_LOG_LEN, "%s %d/%d, %s %d, %s %d.",
                     tr("No reusable or free country slots. Used", "没有可复用或空闲国家槽。已用"),
                     civ_count, MAX_CIVS, tr("alive", "存活"), civilization_alive_count(),
                     tr("reusable", "可复用"), civilization_reusable_slot_count());
            break;
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No valid capital region.", "没有有效首都区域。"));
            break;
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No splittable non-capital region.", "没有可拆分的非首都区域。"));
            break;
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Only capital/core region remains.", "只剩首都/核心区域。"));
            break;
        case COLLAPSE_BLOCK_CITY_CAP:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("City limit reached.", "城市数量已达上限。"));
            break;
        default:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Unknown collapse blocker.", "未知崩溃阻碍。"));
            break;
    }
    return buffer;
}

static void reset_vassal_hits(void) {
    int i;
    last_vassal_count = 0;
    for (i = 0; i < MAX_CIVS; i++) {
        SetRectEmpty(&last_vassal_buttons[i]);
        last_vassal_ids[i] = -1;
    }
}

void country_overview_actions_reset_hit(void) {
    last_civil_unrest_enabled = 0;
    SetRectEmpty(&last_civil_unrest_button);
    reset_vassal_hits();
}

int country_overview_actions_height(int civ_id) {
    int direct = vassal_direct_count(civ_id);
    int is_vassal = vassal_overlord(civ_id) >= 0;
    int rows = is_vassal ? 1 : max(1, direct);
    return 40 + rows * 36 + 32;
}

int country_overview_vassal_actions_height(int civ_id) {
    int direct = vassal_direct_count(civ_id);
    int is_vassal = vassal_overlord(civ_id) >= 0;
    return (is_vassal ? 1 : max(1, direct)) * 36;
}

int country_overview_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y) {
    return last_civil_unrest_enabled &&
           point_in_rect_local(viewport, mouse_x, mouse_y) &&
           point_in_rect_local(last_civil_unrest_button, mouse_x, mouse_y);
}

int country_overview_vassal_action_hit(RECT viewport, int mouse_x, int mouse_y) {
    int i;
    if (!point_in_rect_local(viewport, mouse_x, mouse_y)) return -1;
    for (i = 0; i < last_vassal_count; i++) {
        if (point_in_rect_local(last_vassal_buttons[i], mouse_x, mouse_y)) return last_vassal_ids[i];
    }
    return -1;
}

static void draw_button(HDC hdc, RECT button, const char *text, int enabled) {
    fill_rect(hdc, button, enabled ? RGB(82, 92, 78) : RGB(58, 62, 64));
    draw_text_rect(hdc, button, text, enabled ? RGB(244, 248, 238) : ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id) {
    int can_trigger = collapse_can_trigger(civ_id);
    const char *reason = collapse_block_reason_ui(civ_id);
    RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 34};

    last_civil_unrest_button = button;
    last_civil_unrest_enabled = can_trigger;
    fill_rect(hdc, button, can_trigger ? RGB(112, 45, 45) : RGB(65, 55, 55));
    draw_center_text(hdc, button, tr("Civil Unrest", "内乱"),
                     can_trigger ? RGB(255, 236, 226) : ui_theme_color(UI_COLOR_TEXT_DIM));
    cursor->y += 40;
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "无法崩溃"), reason);
}

static void draw_vassal_action_buttons(HDC hdc, UiCursor *cursor, int civ_id) {
    int overlord = vassal_overlord(civ_id);
    int ids[MAX_CIVS];
    int count;
    int i;
    char text[160];

    reset_vassal_hits();
    if (overlord >= 0) {
        RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 32};
        snprintf(text, sizeof(text), "%s", tr("Independence", "独立"));
        last_vassal_buttons[0] = button;
        last_vassal_ids[0] = civ_id;
        last_vassal_count = 1;
        draw_button(hdc, button, text, 1);
        cursor->y += 36;
        return;
    }

    count = vassal_collect_direct(civ_id, ids, MAX_CIVS);
    if (count <= 0) {
        RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 32};
        draw_button(hdc, button, tr("No vassals", "无附庸"), 0);
        cursor->y += 36;
        return;
    }
    for (i = 0; i < count && i < MAX_CIVS; i++) {
        RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 32};
        snprintf(text, sizeof(text), "%s %.96s", tr("Release", "释放"), civilization_display_name(ids[i]));
        last_vassal_buttons[last_vassal_count] = button;
        last_vassal_ids[last_vassal_count++] = ids[i];
        draw_button(hdc, button, text, 1);
        cursor->y += 36;
    }
}

void draw_country_overview_actions(HDC hdc, UiCursor *cursor, int civ_id) {
    draw_civil_unrest_action(hdc, cursor, civ_id);
    draw_vassal_action_buttons(hdc, cursor, civ_id);
}

void country_overview_vassal_actions_reset_hit(void) {
    reset_vassal_hits();
}

void draw_country_overview_vassal_actions(HDC hdc, UiCursor *cursor, int civ_id) {
    draw_vassal_action_buttons(hdc, cursor, civ_id);
}
