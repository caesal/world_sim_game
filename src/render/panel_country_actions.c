#include "render/panel_country_actions.h"

#include "render/render_common.h"
#include "render/snapshot_ui.h"
#include "sim/collapse.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static RECT last_civil_unrest_button;
static int last_civil_unrest_enabled;
#define VASSAL_HIT_RECTS_MAX (MAX_CIVS * 3)
static RECT last_vassal_buttons[VASSAL_HIT_RECTS_MAX];
static int last_vassal_ids[VASSAL_HIT_RECTS_MAX];
static CountryVassalActionType last_vassal_actions[VASSAL_HIT_RECTS_MAX];
static int last_vassal_count;

static int actions_overlord(int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    return civ ? civ->overlord : -1;
}

static int actions_direct_count(int civ_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i, count = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (snapshot->civs[i].alive && snapshot->civs[i].overlord == civ_id) count++;
    }
    return count;
}

static int actions_collect_direct(int civ_id, int *out, int max_out) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i, count = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (!snapshot->civs[i].alive || snapshot->civs[i].overlord != civ_id) continue;
        if (count < max_out) out[count] = i;
        count++;
    }
    return min(count, max_out);
}

static const char *collapse_block_reason_ui(int civ_id) {
    static char buffers[4][EVENT_LOG_LEN];
    static int index;
    const RenderSnapshot *snapshot = snapshot_ui_current();
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    char *buffer = buffers[index++ % 4];
    int reason = civ ? civ->collapse_block_reason : COLLAPSE_BLOCK_NOT_ALIVE;

    switch (reason) {
        case COLLAPSE_BLOCK_NONE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Ready.", "就绪。"));
            break;
        case COLLAPSE_BLOCK_NOT_ALIVE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Country is invalid or has fallen.",
                                                     "国家无效或已经灭亡。"));
            break;
        case COLLAPSE_BLOCK_MAX_CIVS:
            snprintf(buffer, EVENT_LOG_LEN, "%s %d/%d, %s %d, %s %d.",
                     tr("No reusable or free country slots. Used",
                        "没有可复用或空闲国家槽。已用"),
                     snapshot ? snapshot->civ_count : 0, MAX_CIVS,
                     tr("alive", "存活"), snapshot ? snapshot->civ_alive_count : 0,
                     tr("reusable", "可复用"), snapshot ? snapshot->civ_reusable_slot_count : 0);
            break;
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No valid capital region.",
                                                     "没有有效首都区域。"));
            break;
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No splittable non-capital region.",
                                                     "没有可拆分的非首都区域。"));
            break;
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Only capital/core region remains.",
                                                     "只剩首都/核心区域。"));
            break;
        case COLLAPSE_BLOCK_CITY_CAP:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("City limit reached.", "城市数量已达上限。"));
            break;
        default:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Unknown collapse blocker.",
                                                     "未知崩溃阻碍。"));
            break;
    }
    return buffer;
}

static void reset_vassal_hits(void) {
    int i;
    last_vassal_count = 0;
    for (i = 0; i < VASSAL_HIT_RECTS_MAX; i++) {
        SetRectEmpty(&last_vassal_buttons[i]);
        last_vassal_ids[i] = -1;
        last_vassal_actions[i] = COUNTRY_VASSAL_ACTION_NONE;
    }
}

static void record_vassal_hit(RECT button, int vassal_id, CountryVassalActionType action) {
    if (last_vassal_count >= VASSAL_HIT_RECTS_MAX) return;
    last_vassal_buttons[last_vassal_count] = button;
    last_vassal_ids[last_vassal_count] = vassal_id;
    last_vassal_actions[last_vassal_count] = action;
    last_vassal_count++;
}

void country_overview_actions_reset_hit(void) {
    last_civil_unrest_enabled = 0;
    SetRectEmpty(&last_civil_unrest_button);
    reset_vassal_hits();
}

int country_overview_actions_height(int civ_id) {
    int direct = actions_direct_count(civ_id);
    int is_vassal = actions_overlord(civ_id) >= 0;
    int rows = is_vassal ? 1 : max(1, direct);
    return 40 + rows * 36 + 32;
}

int country_overview_vassal_actions_height(int civ_id) {
    int direct = actions_direct_count(civ_id);
    int is_vassal = actions_overlord(civ_id) >= 0;
    return (is_vassal ? 1 : max(1, direct)) * 36;
}

int country_overview_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y) {
    return last_civil_unrest_enabled &&
           point_in_rect_local(viewport, mouse_x, mouse_y) &&
           point_in_rect_local(last_civil_unrest_button, mouse_x, mouse_y);
}

CountryVassalActionHit country_overview_vassal_action_hit(RECT viewport, int mouse_x, int mouse_y) {
    int i;
    CountryVassalActionHit result = {COUNTRY_VASSAL_ACTION_NONE, -1};
    if (!point_in_rect_local(viewport, mouse_x, mouse_y)) return result;
    for (i = 0; i < last_vassal_count; i++) {
        if (point_in_rect_local(last_vassal_buttons[i], mouse_x, mouse_y)) {
            result.action = last_vassal_actions[i];
            result.vassal_id = last_vassal_ids[i];
            return result;
        }
    }
    return result;
}

static void draw_button(HDC hdc, RECT button, const char *text, int enabled) {
    ui_clay_draw_pill_button(hdc, button, text,
                             ui_clay_state_for_rect(button, hover_x, hover_y, 0, !enabled));
}

static COLORREF colorref_from_color32(Color32 color) {
    return RGB((int)(color & 0xff),
               (int)((color >> 8) & 0xff),
               (int)((color >> 16) & 0xff));
}

static int perceived_luminance(COLORREF color) {
    return (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
}

static COLORREF readable_text_for_fill(COLORREF fill) {
    return perceived_luminance(fill) >= 150 ? RGB(16, 20, 22) : RGB(248, 246, 232);
}

static COLORREF border_for_fill(COLORREF fill) {
    return perceived_luminance(fill) >= 150 ? blend_color(fill, RGB(20, 24, 26), 38)
                                            : blend_color(fill, RGB(246, 242, 220), 30);
}

static void draw_cell_border(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return;
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

static void draw_vassal_name_cell(HDC hdc, RECT rect, int vassal_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(vassal_id);
    COLORREF fill = civ ? colorref_from_color32(civ->color) : ui_theme_color(UI_COLOR_PANEL_SOFT);
    COLORREF text = readable_text_for_fill(fill);
    fill_rect(hdc, rect, fill);
    draw_cell_border(hdc, rect, border_for_fill(fill));
    rect.left += 8;
    rect.right -= 6;
    draw_text_rect(hdc, rect, snapshot_ui_civ_name(vassal_id), text,
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    int can_trigger = civ ? civ->collapse_can_trigger : 0;
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
    int overlord = actions_overlord(civ_id);
    int ids[MAX_CIVS];
    int count, i;
    char text[160];

    reset_vassal_hits();
    if (overlord >= 0) {
        RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 32};
        snprintf(text, sizeof(text), "%s", tr("Independence", "独立"));
        record_vassal_hit(button, civ_id, COUNTRY_VASSAL_ACTION_RELEASE);
        draw_button(hdc, button, text, 1);
        cursor->y += 36;
        return;
    }
    count = actions_collect_direct(civ_id, ids, MAX_CIVS);
    if (count <= 0) {
        RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 32};
        draw_button(hdc, button, tr("No vassals", "无附庸"), 0);
        cursor->y += 36;
        return;
    }
    for (i = 0; i < count && i < MAX_CIVS; i++) {
        int gap = 4;
        int min_action_w = 62;
        int min_name_w;
        int name_w = cursor->width * 52 / 100;
        int max_name_w = cursor->width - min_action_w * 2 - gap * 2;
        int action_w;
        RECT name_rect;
        RECT release_button;
        RECT annex_button;
        if (max_name_w < 48) max_name_w = 48;
        min_name_w = max_name_w < 82 ? max_name_w : 82;
        if (name_w < min_name_w) name_w = min_name_w;
        if (name_w > max_name_w) name_w = max_name_w;
        action_w = (cursor->width - name_w - gap * 2) / 2;
        if (action_w < 44) action_w = 44;
        name_rect = (RECT){cursor->x, cursor->y + 4, cursor->x + name_w, cursor->y + 32};
        release_button = (RECT){name_rect.right + gap, cursor->y + 4,
                                name_rect.right + gap + action_w, cursor->y + 32};
        annex_button = (RECT){release_button.right + gap, cursor->y + 4,
                              cursor->x + cursor->width, cursor->y + 32};
        draw_vassal_name_cell(hdc, name_rect, ids[i]);
        record_vassal_hit(name_rect, ids[i], COUNTRY_VASSAL_ACTION_SELECT);
        snprintf(text, sizeof(text), "%s", tr("Release", "释放"));
        record_vassal_hit(release_button, ids[i], COUNTRY_VASSAL_ACTION_RELEASE);
        draw_button(hdc, release_button, text, 1);
        snprintf(text, sizeof(text), "%s", tr("Annex", "吞并"));
        record_vassal_hit(annex_button, ids[i], COUNTRY_VASSAL_ACTION_ANNEX);
        draw_button(hdc, annex_button, text, 1);
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
