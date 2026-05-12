#include "ui/ui_selection.h"

#include "core/country_focus.h"
#include "core/game_state.h"
#include "ui/ui_forms.h"
#include "ui/ui_types.h"

static int valid_civ_id(int civ_id) { return civ_id >= 0 && civ_id < civ_count; }
static int now_ms(void) { return (int)GetTickCount(); }

static int pulse_active_since(int start_ms) {
    if (start_ms <= 0) return 0;
    return now_ms() - start_ms < 3000;
}

static void select_civ_base(int civ_id, UiSelectSource source) {
    (void)source;
    if (!valid_civ_id(civ_id)) return;
    if (selected_civ != civ_id) previous_selected_civ = selected_civ;
    selected_civ = civ_id;
    map_highlight_civ = -1;
    selected_civ_pulse_start_ms = now_ms();
    map_highlight_pulse_start_ms = 0;
    ui_forms_write_civ(civ_id);
}

void ui_select_civ(int civ_id, UiSelectSource source) {
    if (!valid_civ_id(civ_id)) return;
    select_civ_base(civ_id, source);
    country_detail_subtab = COUNTRY_DETAIL_OVERVIEW;
    country_detail_scroll_offsets[COUNTRY_DETAIL_OVERVIEW] = 0;
    country_detail_scroll_offset = 0;
}

void ui_select_civ_preserve_view(int civ_id, UiSelectSource source) {
    int tab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
    if (!valid_civ_id(civ_id)) return;
    country_detail_subtab = tab;
    select_civ_base(civ_id, source);
    country_detail_scroll_offsets[tab] = 0;
    country_detail_scroll_offset = 0;
}

void ui_clear_selected_civ(UiSelectSource source) {
    (void)source;
    selected_civ = -1;
    previous_selected_civ = -1;
    map_highlight_civ = -1;
    selected_civ_pulse_start_ms = 0;
    map_highlight_pulse_start_ms = 0;
    country_detail_scroll_offset = 0;
}

void ui_highlight_civ(int civ_id, UiHighlightSource source) {
    (void)source;
    if (!valid_civ_id(civ_id)) return;
    map_highlight_civ = civ_id;
    map_highlight_pulse_start_ms = now_ms();
}

void ui_clear_map_highlight(void) {
    map_highlight_civ = -1;
    map_highlight_pulse_start_ms = 0;
}

int ui_locate_civ(HWND hwnd, int civ_id) {
    (void)hwnd;
    if (!valid_civ_id(civ_id) || !country_focus_point(civ_id, NULL, NULL)) return 0;
    map_highlight_civ = civ_id;
    map_highlight_pulse_start_ms = now_ms();
    return 1;
}

int ui_highlight_pulse_active(void) {
    return pulse_active_since(selected_civ_pulse_start_ms) ||
           pulse_active_since(map_highlight_pulse_start_ms);
}
