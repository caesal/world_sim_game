#include "render_panel_internal.h"

#include "core/profiler.h"
#include "game/game_loop.h"
#include "render/panel_alliance.h"
#include "render/panel_map_legend_glyphs.h"
#include "render/panel_map_water_legend.h"
#include "render/panel_map_speed_badge.h"
#include "render/snapshot_ui.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_theme.h"

void draw_side_panel_handle(HDC hdc, RECT client) {
    RECT handle = get_side_panel_handle_rect(client);
    int hot = point_in_rect(handle, hover_x, hover_y);
    UiClayState state = ui_clay_state_from_flags(
        hot, hot && (GetKeyState(VK_LBUTTON) & 0x8000), 0, 0);

    ui_clay_draw_icon_button(hdc, handle, side_panel_collapsed ? "<" : ">", state);
}

static int alliance_view_should_draw_country_panel(void) {
    const SnapshotCiv *civ;
    if (display_mode != DISPLAY_ALLIANCE || selected_alliance_id >= 0 || selected_civ < 0) return 0;
    civ = snapshot_ui_civ(selected_civ);
    return civ && civ->alive && civ->alliance_display_id < 0;
}

void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontW(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT body_font = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT old_font;

    if (side_panel_collapsed) {
        draw_side_panel_handle(hdc, client);
        DeleteObject(title_font);
        DeleteObject(body_font);
        return;
    }
    ui_clay_draw_panel(hdc, panel, UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, divider, ui_theme_color(UI_COLOR_PANEL_LINE));
    draw_panel_tabs(hdc, client);

    old_font = SelectObject(hdc, title_font);
    if (panel_tab == PANEL_COUNTRY && display_mode == DISPLAY_ALLIANCE &&
        !alliance_view_should_draw_country_panel())
        draw_alliance_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_COUNTRY) draw_country_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_POPULATION) draw_population_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_PLAGUE) draw_plague_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_WORLD) draw_worldgen_panel(hdc, client, x, title_font, body_font);
    else draw_debug_panel(hdc, client, x, title_font, body_font);

    SelectObject(hdc, old_font);
    draw_side_panel_handle(hdc, client);
    DeleteObject(title_font);
    DeleteObject(body_font);
}

void draw_bottom_bar(HDC hdc, RECT client) {
    int panel_w = ui_side_panel_reserved_width();
    RECT bar = {0, client.bottom - BOTTOM_BAR_H, client.right - panel_w, client.bottom};
    RECT play = get_play_button_rect(client);
    int pending = game_loop_pending_months();
    RuntimeProfilerSnapshot profiler;
    int play_hot = point_in_rect_local(play, hover_x, hover_y);
    UiClayState play_state = ui_clay_state_from_flags(
        play_hot, ui_pressed_control_is_active(UI_PRESSED_PLAY, 0) ||
        (play_hot && (GetKeyState(VK_LBUTTON) & 0x8000)), auto_run, 0);
    int i;

    profiler_snapshot(&profiler);

    ui_clay_draw_bar_shell(hdc, bar);
    ui_clay_draw_icon_button(hdc, play, auto_run ? "||" : ">", play_state);

    for (i = 0; i < SPEED_COUNT; i++) {
        RECT button = get_speed_button_rect(client, i);
        int hot = point_in_rect_local(button, hover_x, hover_y);
        UiClayState state = ui_clay_state_from_flags(
            hot, ui_pressed_control_is_active(UI_PRESSED_SPEED, i) ||
            (hot && (GetKeyState(VK_LBUTTON) & 0x8000)), i == speed_index, 0);
        ui_clay_draw_icon_button(hdc, button, speed_button_icon(i), state);
    }

    panel_map_draw_bottom_status_chips(hdc, client, panel_w, ui_language,
                                       profiler.render_avg_ms, pending, auto_run,
                                       game_loop_simulation_overloaded());
}

void draw_map_frame_overlay(HDC hdc, RECT client) {
    (void)client;
    panel_map_draw_actual_speed_badge(hdc, client, game_loop_actual_ms_per_month());
}

static void draw_legend_item(HDC hdc, int x, int y, COLORREF color, const char *name) {
    RECT swatch = {x, y + 3, x + 16, y + 15};
    fill_rect(hdc, swatch, color);
    draw_text_line(hdc, x + 22, y, name, RGB(232, 238, 242));
}

static void draw_legend_group(HDC hdc, int x, int y, const char *name) {
    draw_text_line(hdc, x, y, name, RGB(156, 174, 184));
}

static int alliance_legend_army(const RenderSnapshot *snapshot, const AllianceSnapshotRecord *alliance) {
    int i, total = 0;
    for (i = 0; snapshot && alliance && i < alliance->member_count && i < MAX_CIVS; i++) {
        int member = alliance->members[i];
        if (member >= 0 && member < snapshot->civ_count && snapshot->civs[member].alive)
            total += snapshot->civs[member].current_soldiers;
    }
    return total;
}

int panel_map_probe_alliance_legend_before(const RenderSnapshot *snapshot, int a_index, int b_index) {
    const AllianceSnapshotRecord *a;
    const AllianceSnapshotRecord *b;
    int army_a, army_b;
    if (!snapshot || a_index < 0 || b_index < 0 ||
        a_index >= snapshot->alliance_count || b_index >= snapshot->alliance_count) return 0;
    a = &snapshot->alliances[a_index];
    b = &snapshot->alliances[b_index];
    if (a->member_count != b->member_count) return a->member_count > b->member_count;
    army_a = alliance_legend_army(snapshot, a);
    army_b = alliance_legend_army(snapshot, b);
    if (army_a != army_b) return army_a > army_b;
    return a->id < b->id;
}

static int draw_alliance_legend(HDC hdc, int x, int y, int line_h) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i, active_count = 0;
    int order[ALLIANCE_MAX];
    int drawn = 0;
    draw_legend_group(hdc, x, y, tr("Alliances", "同盟"));
    y += line_h;
    if (!snapshot || snapshot->alliance_count <= 0) {
        draw_legend_item(hdc, x, y, RGB(124, 132, 136), tr("No alliances", "无同盟"));
        return y + line_h;
    }
    for (i = 0; i < snapshot->alliance_count && i < ALLIANCE_MAX; i++) {
        int j;
        if (!snapshot->alliances[i].active) continue;
        for (j = active_count; j > 0; j--) {
            if (!panel_map_probe_alliance_legend_before(snapshot, i, order[j - 1])) break;
            order[j] = order[j - 1];
        }
        order[j] = i;
        active_count++;
    }
    if (active_count <= 0) {
        draw_legend_item(hdc, x, y, RGB(124, 132, 136), tr("No alliances", "无同盟"));
        return y + line_h;
    }
    for (i = 0; i < active_count && drawn < 8; i++) {
        const AllianceSnapshotRecord *alliance = &snapshot->alliances[order[i]];
        char label[128];
        const char *name = ui_language == UI_LANG_ZH ? alliance->name_zh : alliance->name_en;
        snprintf(label, sizeof(label), "%s (%d)", name, alliance->member_count);
        draw_legend_item(hdc, x, y, (COLORREF)alliance->color, label);
        y += line_h;
        drawn++;
    }
    if (drawn < active_count) {
        char more[48];
        snprintf(more, sizeof(more), "+%d %s", active_count - drawn, tr("more", "更多"));
        draw_text_line(hdc, x + 22, y, more, RGB(174, 186, 190));
        y += line_h;
    }
    return y;
}

static void draw_legend_background(HDC hdc, RECT box) {
    HBRUSH border = CreateSolidBrush(ui_theme_color(UI_COLOR_PANEL_LINE));
    fill_rect_alpha(hdc, box, ui_theme_color(UI_COLOR_PANEL),
                    (BYTE)ui_theme_overlay_alpha());
    FrameRect(hdc, &box, border);
    DeleteObject(border);
}

void draw_map_legend(HDC hdc, RECT client) {
    const Geography geographies[] = {
        GEO_PLAIN, GEO_HILL, GEO_MOUNTAIN, GEO_PLATEAU,
        GEO_BASIN, GEO_CANYON, GEO_VOLCANO, GEO_DELTA,
        GEO_WETLAND, GEO_OASIS, GEO_ISLAND
    };
    const Climate climates[] = {
        CLIMATE_TROPICAL_RAINFOREST, CLIMATE_TROPICAL_MONSOON, CLIMATE_TROPICAL_SAVANNA,
        CLIMATE_DESERT, CLIMATE_SEMI_ARID, CLIMATE_MEDITERRANEAN, CLIMATE_OCEANIC,
        CLIMATE_TEMPERATE_MONSOON, CLIMATE_CONTINENTAL, CLIMATE_SUBARCTIC, CLIMATE_TUNDRA,
        CLIMATE_ICE_CAP, CLIMATE_ALPINE, CLIMATE_HIGHLAND_PLATEAU
    };
    int geo_count = (int)(sizeof(geographies) / sizeof(geographies[0]));
    int climate_count = (int)(sizeof(climates) / sizeof(climates[0]));
    int line_h = 20;
    int x;
    int y;
    int i;
    int route_only = display_mode == DISPLAY_ROUTE_POTENTIAL;
    int show_geography = !route_only && display_mode != DISPLAY_CLIMATE;
    int show_climate = display_mode == DISPLAY_POLITICAL || display_mode == DISPLAY_REGIONS ||
                       display_mode == DISPLAY_ALL ||
                       display_mode == DISPLAY_CLIMATE;
    int show_routes = route_only;
    int show_city_glyphs = display_mode == DISPLAY_POLITICAL;
    RECT box = get_map_legend_box_rect(client);
    RECT toggle = get_map_legend_toggle_rect(client);
    UiClayState toggle_state;
    int saved_dc;
    int collapsed;

    if (IsRectEmpty(&box)) return;
    collapsed = map_legend_collapsed || (box.bottom - box.top <= 40);

    draw_legend_background(hdc, box);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, box.left, box.top, box.right, box.bottom);
    toggle_state = ui_clay_state_for_rect(toggle, hover_x, hover_y, 0, 0);
    ui_clay_draw_icon_button(hdc, toggle, collapsed ? "^" : "v", toggle_state);
    if (collapsed) {
        RestoreDC(hdc, saved_dc);
        return;
    }
    draw_text_line(hdc, box.left + 10, box.top + 8, tr("Map Legend", "地图图例"), RGB(245, 245, 245));

    x = box.left + 10;
    y = box.top + 30;

    if (display_mode == DISPLAY_ALLIANCE) {
        draw_alliance_legend(hdc, x, y, line_h);
        RestoreDC(hdc, saved_dc);
        return;
    }

    if (show_routes && !show_geography && !show_climate) {
        draw_legend_group(hdc, x, y, tr("Routes", "航道"));
        y += line_h;
        draw_legend_item(hdc, x, y, RGB(240, 238, 218), tr("Shallow route", "浅海航道"));
        draw_legend_item(hdc, x, y + line_h, RGB(70, 74, 78), tr("Deep route", "深海航道"));
        RestoreDC(hdc, saved_dc);
        return;
    }

    if (show_geography) {
        y = panel_map_water_legend_draw(hdc, x, y, line_h);
        draw_legend_group(hdc, x, y - line_h, tr("Terrain", "地形"));
        for (i = 0; i < geo_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, geography_color(geographies[i]), geography_name(geographies[i]));
        }
    }

    if (show_climate) {
        if (show_geography) {
            x = box.left + 190;
            y = box.top + 30;
        }
        draw_legend_group(hdc, x, y, tr("Climate", "气候"));
        y += line_h;
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
        if (show_routes) {
            y += climate_count * line_h + line_h;
            draw_legend_group(hdc, x, y, tr("Routes", "航道"));
            y += line_h;
            draw_legend_item(hdc, x, y, RGB(240, 238, 218), tr("Shallow route", "浅海航道"));
            draw_legend_item(hdc, x, y + line_h, RGB(70, 74, 78), tr("Deep route", "深海航道"));
        }
    }
    if (show_city_glyphs) {
        x = box.left + 370;
        y = box.top + 30;
        panel_map_legend_draw_glyph_item(hdc, x, y, ICON_CITY_OUTPOST, 0, 0, tr("Outpost", "据点"));
        panel_map_legend_draw_glyph_item(hdc, x, y + line_h, ICON_CITY_VILLAGE, 0, 0, tr("Village", "村落"));
        panel_map_legend_draw_glyph_item(hdc, x, y + line_h * 2, ICON_CITY_TOWN, 0, 0, tr("Town", "城镇"));
        panel_map_legend_draw_glyph_item(hdc, x, y + line_h * 3, ICON_CITY_STAGE, 0, 0, tr("City", "城市"));
        panel_map_legend_draw_glyph_item(hdc, x, y + line_h * 4, ICON_HARBOR, 1, 0, tr("Harbor", "港口"));
        panel_map_legend_draw_capital_pair_item(hdc, x, y + line_h * 5);
    }
    RestoreDC(hdc, saved_dc);
}
