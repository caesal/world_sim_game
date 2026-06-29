#include "render_panel_internal.h"

#include "core/profiler.h"
#include "game/game_loop.h"
#include "render/panel_alliance.h"
#include "render/panel_map_speed_badge.h"
#include "render/snapshot_ui.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_theme.h"

static void draw_side_panel_handle(HDC hdc, RECT client) {
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
    RECT viewport = get_map_viewport_rect(client);
    MapLayout layout = get_map_layout(client);
    RECT frame = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};
    HBRUSH outer = CreateSolidBrush(RGB(24, 28, 32));
    HBRUSH inner = CreateSolidBrush(RGB(132, 116, 82));
    HBRUSH line = CreateSolidBrush(RGB(215, 196, 142));
    RECT shade = {frame.left + 4, frame.top + 4, frame.right + 4, frame.bottom + 4};
    int saved = SaveDC(hdc);

    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    FrameRect(hdc, &shade, outer);
    FrameRect(hdc, &frame, inner);
    InflateRect(&frame, -2, -2);
    FrameRect(hdc, &frame, line);
    RestoreDC(hdc, saved);
    DeleteObject(outer);
    DeleteObject(inner);
    DeleteObject(line);
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

static RECT legend_centered_rect(int cx, int cy, int size) {
    RECT rect = {cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
    return rect;
}

static void draw_legend_marker_ellipse(HDC hdc, RECT rect, COLORREF fill, COLORREF outline, int width) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, width, outline);
    HBRUSH old_brush = SelectObject(hdc, brush);
    HPEN old_pen = SelectObject(hdc, pen);

    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_legend_city_stage_glyph(HDC hdc, RECT rect, IconId icon) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int stroke = clamp(min(w, h) / 7, 1, 3);
    int inset = max(1, min(w, h) / 6);
    RECT body = {rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset};
    HPEN pen = CreatePen(PS_SOLID, stroke, RGB(31, 28, 20));
    HBRUSH brush = CreateSolidBrush(RGB(36, 32, 22));
    HPEN old_pen = SelectObject(hdc, pen);
    HBRUSH old_brush = SelectObject(hdc, brush);

    if (icon == ICON_CITY_OUTPOST) {
        POINT tent[3] = {{body.left + w / 5, body.bottom},
                         {(body.left + body.right) / 2, body.top},
                         {body.right - w / 5, body.bottom}};
        Polygon(hdc, tent, 3);
    } else if (icon == ICON_CITY_VILLAGE) {
        POINT roof[3] = {{body.left, body.top + h / 2},
                         {(body.left + body.right) / 2, body.top},
                         {body.right, body.top + h / 2}};
        Polygon(hdc, roof, 3);
        Rectangle(hdc, body.left + w / 8, body.top + h / 2, body.right - w / 8, body.bottom);
    } else if (icon == ICON_CITY_TOWN) {
        POINT left_roof[3] = {{body.left, body.top + h / 2}, {body.left + w / 4, body.top + h / 5},
                              {body.left + w / 2, body.top + h / 2}};
        POINT right_roof[3] = {{body.left + w / 2, body.top + h / 2}, {body.right - w / 4, body.top},
                               {body.right, body.top + h / 2}};
        Polygon(hdc, left_roof, 3);
        Polygon(hdc, right_roof, 3);
        Rectangle(hdc, body.left + w / 12, body.top + h / 2, body.right - w / 12, body.bottom);
    } else {
        Rectangle(hdc, body.left, body.top + h / 3, body.right, body.bottom);
        Rectangle(hdc, body.left, body.top + h / 5, body.left + w / 5, body.top + h / 2);
        Rectangle(hdc, (body.left + body.right) / 2 - w / 10, body.top,
                  (body.left + body.right) / 2 + w / 10, body.top + h / 2);
        Rectangle(hdc, body.right - w / 5, body.top + h / 5, body.right, body.top + h / 2);
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_legend_harbor_glyph(HDC hdc, RECT rect) {
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    int s = min(rect.right - rect.left, rect.bottom - rect.top);
    int top = cy - s / 3;
    int bottom = cy + s / 3;
    int arm = s / 3;
    HPEN pen = CreatePen(PS_SOLID, clamp(s / 6, 2, 4), RGB(20, 38, 38));
    HBRUSH old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN old_pen = SelectObject(hdc, pen);

    Ellipse(hdc, cx - s / 8, top - s / 8, cx + s / 8, top + s / 8);
    MoveToEx(hdc, cx, top + s / 8, NULL);
    LineTo(hdc, cx, bottom);
    MoveToEx(hdc, cx - arm, cy, NULL);
    LineTo(hdc, cx + arm, cy);
    MoveToEx(hdc, cx - arm, bottom - s / 7, NULL);
    LineTo(hdc, cx, bottom);
    LineTo(hdc, cx + arm, bottom - s / 7);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
}

static void draw_legend_city_marker_sized(HDC hdc, int cx, int cy, IconId icon, int capital,
                                          int plate_size, int glyph_size) {
    RECT plate = legend_centered_rect(cx, cy, plate_size);
    RECT glyph = legend_centered_rect(cx, cy, glyph_size);

    draw_legend_marker_ellipse(hdc, plate, RGB(232, 218, 176), RGB(28, 27, 23),
                               capital && plate_size >= 18 ? 2 : 1);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -max(2, plate_size / 6), -max(2, plate_size / 6));
        draw_legend_marker_ellipse(hdc, inner, RGB(241, 229, 188), RGB(28, 27, 23), 1);
    }
    draw_legend_city_stage_glyph(hdc, glyph, icon);
}

static void draw_legend_city_marker(HDC hdc, int cx, int cy, IconId icon, int capital) {
    draw_legend_city_marker_sized(hdc, cx, cy, icon, capital, capital ? 20 : 18, capital ? 13 : 12);
}

static void draw_legend_harbor_marker_sized(HDC hdc, int cx, int cy, int capital,
                                            int plate_size, int glyph_size) {
    RECT plate = legend_centered_rect(cx, cy, plate_size);
    RECT glyph = legend_centered_rect(cx, cy, glyph_size);

    draw_legend_marker_ellipse(hdc, plate, RGB(218, 226, 205), RGB(31, 50, 48), 1);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -max(2, plate_size / 6), -max(2, plate_size / 6));
        draw_legend_marker_ellipse(hdc, inner, RGB(226, 234, 217), RGB(28, 27, 23), 1);
    }
    draw_legend_harbor_glyph(hdc, glyph);
}

static void draw_legend_harbor_marker(HDC hdc, int cx, int cy, int capital) {
    draw_legend_harbor_marker_sized(hdc, cx, cy, capital, capital ? 20 : 18, capital ? 13 : 12);
}

static void draw_legend_glyph_item(HDC hdc, int x, int y, IconId icon, int harbor, int capital, const char *name) {
    RECT label = {x + 54, y, x + 158, y + 20};
    int cx = x + 9;
    int cy = y + 10;

    if (harbor) draw_legend_harbor_marker(hdc, cx, cy, capital);
    else draw_legend_city_marker(hdc, cx, cy, icon, capital);
    draw_text_rect(hdc, label, name, RGB(232, 238, 242),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_legend_capital_pair_item(HDC hdc, int x, int y) {
    RECT label = {x + 54, y, x + 158, y + 20};
    int cy = y + 10;
    draw_legend_city_marker_sized(hdc, x + 9, cy, ICON_CITY_CAPITAL, 1, 18, 12);
    draw_legend_harbor_marker_sized(hdc, x + 33, cy, 1, 18, 12);
    draw_text_rect(hdc, label, tr("Capital", "首都"), RGB(232, 238, 242),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
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
    fill_rect_alpha(hdc, box, ui_theme_color(UI_COLOR_PANEL), 166);
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
        draw_legend_group(hdc, x, y, tr("Water", "水域"));
        y += line_h;
        draw_legend_item(hdc, x, y, RGB(92, 177, 214), tr("Shallow Sea", "浅海"));
        draw_legend_item(hdc, x, y + line_h, RGB(38, 92, 154), tr("Deep Sea", "深海"));
        y += line_h * 3;
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
        draw_legend_glyph_item(hdc, x, y, ICON_CITY_OUTPOST, 0, 0, tr("Outpost", "据点"));
        draw_legend_glyph_item(hdc, x, y + line_h, ICON_CITY_VILLAGE, 0, 0, tr("Village", "村落"));
        draw_legend_glyph_item(hdc, x, y + line_h * 2, ICON_CITY_TOWN, 0, 0, tr("Town", "城镇"));
        draw_legend_glyph_item(hdc, x, y + line_h * 3, ICON_CITY_STAGE, 0, 0, tr("City", "城市"));
        draw_legend_glyph_item(hdc, x, y + line_h * 4, ICON_HARBOR, 1, 0, tr("Harbor", "港口"));
        draw_legend_capital_pair_item(hdc, x, y + line_h * 5);
    }
    RestoreDC(hdc, saved_dc);
}
