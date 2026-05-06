#include "render_common.h"

void fill_rect(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void fill_rect_alpha(HDC hdc, RECT rect, COLORREF color, BYTE alpha) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HDC memory_dc;
    BITMAPINFO info;
    void *bits = NULL;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int pixel;
    unsigned int *pixels;
    int count;
    int i;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, 0};

    if (width <= 0 || height <= 0) return;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        fill_rect(hdc, rect, color);
        return;
    }

    pixel = (unsigned int)GetRValue(color) | ((unsigned int)GetGValue(color) << 8) |
            ((unsigned int)GetBValue(color) << 16);
    pixels = (unsigned int *)bits;
    count = width * height;
    for (i = 0; i < count; i++) pixels[i] = pixel;

    memory_dc = CreateCompatibleDC(hdc);
    old_bitmap = SelectObject(memory_dc, bitmap);
    AlphaBlend(hdc, rect.left, rect.top, width, height, memory_dc, 0, 0, width, height, blend);
    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
    DeleteObject(bitmap);
}

void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color) {
    WCHAR wide_text[512];
    int len;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) TextOutW(hdc, x, y, wide_text, len - 1);
}

void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color) {
    WCHAR wide_text[512];
    int len;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) DrawTextW(hdc, wide_text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void draw_text_rect(HDC hdc, RECT rect, const char *text, COLORREF color, unsigned int format) {
    WCHAR wide_text[512];
    int len;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) DrawTextW(hdc, wide_text, -1, &rect, format);
}

void measure_text_utf8(HDC hdc, const char *text, SIZE *out_size) {
    WCHAR wide_text[512];
    int len;
    if (!out_size) return;
    out_size->cx = 0;
    out_size->cy = 0;
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) GetTextExtentPoint32W(hdc, wide_text, len - 1, out_size);
}

const char *tr(const char *en, const char *zh) {
    return ui_language == UI_LANG_ZH ? zh : en;
}

const char *geography_name(Geography geography) {
    if (geography < 0 || geography >= GEO_COUNT) return tr("Unknown", "未知");
    return localized_text(GEOGRAPHY_RULES[geography].name, ui_language);
}

const char *climate_name(Climate climate) {
    if (climate < 0 || climate >= CLIMATE_COUNT) return tr("Unknown", "未知");
    return localized_text(CLIMATE_RULES[climate].name, ui_language);
}

const char *ecology_name(Ecology ecology) {
    if (ecology < 0 || ecology >= ECO_COUNT) return tr("Unknown", "未知");
    return localized_text(ECOLOGY_RULES[ecology].name, ui_language);
}

const char *resource_name(ResourceFeature resource) {
    if (resource < 0 || resource >= RESOURCE_FEATURE_COUNT) return tr("Unknown", "未知");
    return localized_text(RESOURCE_FEATURE_RULES[resource].name, ui_language);
}

COLORREF geography_color(Geography geography) {
    switch (geography) {
        case GEO_OCEAN: return RGB(74, 139, 201);
        case GEO_COAST: return RGB(199, 224, 201);
        case GEO_PLAIN: return RGB(189, 190, 157);
        case GEO_HILL: return RGB(140, 125, 88);
        case GEO_MOUNTAIN: return RGB(101, 92, 81);
        case GEO_PLATEAU: return RGB(154, 141, 106);
        case GEO_BASIN: return RGB(165, 166, 135);
        case GEO_CANYON: return RGB(158, 104, 74);
        case GEO_VOLCANO: return RGB(83, 71, 68);
        case GEO_LAKE: return RGB(87, 154, 207);
        case GEO_BAY: return RGB(98, 171, 215);
        case GEO_DELTA: return RGB(134, 177, 111);
        case GEO_WETLAND: return RGB(106, 154, 112);
        case GEO_OASIS: return RGB(118, 178, 118);
        case GEO_ISLAND: return RGB(154, 184, 112);
        case GEO_COUNT: return RGB(0, 0, 0);
        default: return RGB(0, 0, 0);
    }
}

COLORREF climate_color(Climate climate) {
    switch (climate) {
        case CLIMATE_TROPICAL_RAINFOREST: return RGB(30, 126, 52);
        case CLIMATE_TROPICAL_MONSOON: return RGB(74, 164, 62);
        case CLIMATE_TROPICAL_SAVANNA: return RGB(170, 188, 75);
        case CLIMATE_DESERT: return RGB(224, 174, 74);
        case CLIMATE_SEMI_ARID: return RGB(194, 176, 94);
        case CLIMATE_MEDITERRANEAN: return RGB(151, 183, 93);
        case CLIMATE_OCEANIC: return RGB(83, 150, 93);
        case CLIMATE_TEMPERATE_MONSOON: return RGB(96, 170, 83);
        case CLIMATE_CONTINENTAL: return RGB(149, 176, 96);
        case CLIMATE_SUBARCTIC: return RGB(87, 137, 103);
        case CLIMATE_TUNDRA: return RGB(178, 185, 153);
        case CLIMATE_ICE_CAP: return RGB(233, 238, 233);
        case CLIMATE_ALPINE: return RGB(172, 168, 150);
        case CLIMATE_HIGHLAND_PLATEAU: return RGB(164, 154, 120);
        case CLIMATE_COUNT: return RGB(0, 0, 0);
        default: return RGB(0, 0, 0);
    }
}

COLORREF overview_color(int x, int y) {
    COLORREF base = geography_color(world[y][x].geography);
    COLORREF climate = climate_color(world[y][x].climate);
    int blend = is_land(world[y][x].geography) ? 48 : 18;
    COLORREF color = blend_color(base, climate, blend);
    int elev = world[y][x].elevation;

    if (!is_land(world[y][x].geography)) return color;
    if (elev > 55) return blend_color(color, RGB(38, 35, 32), clamp((elev - 55) / 3, 0, 18));
    return blend_color(color, RGB(236, 230, 198), clamp((55 - elev) / 4, 0, 12));
}

int point_in_rect_local(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void draw_icon_text_line(HDC hdc, int x, int y, IconId icon, const char *text, COLORREF color) {
    RECT icon_rect = {x, y, x + 18, y + 18};
    draw_icon(hdc, icon, icon_rect, RGB(90, 110, 130));
    draw_text_line(hdc, x + 26, y, text, color);
}

void format_metric_value(int value, char *buffer, size_t buffer_size) {
    if (value >= 1000000000) snprintf(buffer, buffer_size, "%.1fB", value / 1000000000.0);
    else if (value >= 1000000) snprintf(buffer, buffer_size, "%.1fM", value / 1000000.0);
    else if (value >= 10000) snprintf(buffer, buffer_size, "%dK", value / 1000);
    else snprintf(buffer, buffer_size, "%d", value);
}

void draw_metric_box(HDC hdc, RECT rect, IconId icon, const char *label, int value, COLORREF accent,
                            const char *tooltip_text, const char **active_tooltip) {
    char text[32];
    RECT accent_rect = rect;
    RECT icon_rect = rect;
    RECT label_rect = rect;
    RECT value_rect = rect;

    fill_rect(hdc, rect, RGB(39, 47, 56));
    accent_rect.right = accent_rect.left + 4;
    fill_rect(hdc, accent_rect, accent);
    icon_rect.left += 8;
    icon_rect.top += 3;
    icon_rect.right = icon_rect.left + 22;
    icon_rect.bottom -= 3;
    draw_icon(hdc, icon, icon_rect, accent);
    label_rect.left = rect.left + 36;
    label_rect.right = rect.right - 48;
    draw_center_text(hdc, label_rect, label, RGB(176, 187, 197));
    value_rect.left = rect.right - 48;
    format_metric_value(value, text, sizeof(text));
    draw_center_text(hdc, value_rect, text, RGB(232, 238, 242));
    if (point_in_rect_local(rect, hover_x, hover_y)) *active_tooltip = tooltip_text;
}

const char *metric_label(const char *en, const char *zh) {
    return ui_language == UI_LANG_ZH ? zh : en;
}

RECT metric_grid_rect(int x, int y, int box_w, int box_h, int index) {
    RECT rect;
    int col = index % 4;
    int row = index / 4;

    rect.left = x + col * (box_w + 8);
    rect.top = y + row * (box_h + 6);
    rect.right = rect.left + box_w;
    rect.bottom = rect.top + box_h;
    return rect;
}

void draw_tooltip(HDC hdc, RECT client, const char *tooltip_text) {
    RECT tip;
    SIZE text_size;
    int panel_left = client.right - side_panel_w;
    int width;
    int height = 26;

    if (!tooltip_text || hover_x < 0 || hover_y < 0) return;
    measure_text_utf8(hdc, tooltip_text, &text_size);
    width = clamp(text_size.cx + 24, 120, side_panel_w - FORM_X_PAD * 2);
    tip.left = hover_x + 14;
    tip.top = hover_y + 18;
    tip.right = tip.left + width;
    tip.bottom = tip.top + height;
    if (tip.right > client.right - FORM_X_PAD) {
        tip.right = client.right - FORM_X_PAD;
        tip.left = tip.right - width;
    }
    if (tip.left < panel_left + FORM_X_PAD) {
        tip.left = panel_left + FORM_X_PAD;
        tip.right = tip.left + width;
    }
    if (tip.bottom > client.bottom - 10) {
        tip.bottom = hover_y - 12;
        tip.top = tip.bottom - height;
    }
    if (tip.top < TOP_BAR_H + 8) {
        tip.top = TOP_BAR_H + 8;
        tip.bottom = tip.top + height;
    }
    fill_rect(hdc, tip, RGB(54, 62, 70));
    draw_center_text(hdc, tip, tooltip_text, RGB(236, 241, 245));
}

int visible_tile_bounds(RECT client, MapLayout layout, int *min_x, int *max_x, int *min_y, int *max_y) {
    int view_left = client.left;
    int view_top = TOP_BAR_H;
    int view_right = client.right - side_panel_w;
    int view_bottom = client.bottom - BOTTOM_BAR_H;

    if (layout.draw_w <= 0 || layout.draw_h <= 0) return 0;
    if (view_right <= layout.map_x || view_bottom <= layout.map_y ||
        view_left >= layout.map_x + layout.draw_w || view_top >= layout.map_y + layout.draw_h) {
        return 0;
    }

    *min_x = clamp((view_left - layout.map_x) * MAP_W / layout.draw_w - 2, 0, MAP_W - 1);
    *max_x = clamp((view_right - layout.map_x) * MAP_W / layout.draw_w + 2, 0, MAP_W - 1);
    *min_y = clamp((view_top - layout.map_y) * MAP_H / layout.draw_h - 2, 0, MAP_H - 1);
    *max_y = clamp((view_bottom - layout.map_y) * MAP_H / layout.draw_h + 2, 0, MAP_H - 1);
    return *min_x <= *max_x && *min_y <= *max_y;
}

COLORREF tile_display_color(int x, int y) {
    int owner = world[y][x].owner;
    COLORREF base;

    switch (display_mode) {
        case DISPLAY_CLIMATE:
            base = climate_color(world[y][x].climate);
            break;
        case DISPLAY_GEOGRAPHY:
            base = blend_color(geography_color(world[y][x].geography), overview_color(x, y), 35);
            break;
        case DISPLAY_REGIONS:
            base = overview_color(x, y);
            if (world[y][x].region_id >= 0) {
                int id = world[y][x].region_id;
                COLORREF region_color = RGB(92 + (id * 37) % 112, 105 + (id * 53) % 96, 86 + (id * 29) % 104);
                base = blend_color(base, region_color, 44);
            }
            break;
        case DISPLAY_POLITICAL:
            base = overview_color(x, y);
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                base = blend_color(base, civs[owner].color, 74);
            }
            break;
        case DISPLAY_ALL:
            base = overview_color(x, y);
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                base = blend_color(base, civs[owner].color, 38);
            }
            break;
        case DISPLAY_OVERVIEW:
        default:
            base = overview_color(x, y);
            break;
    }
    return base;
}

int tile_left(MapLayout layout, int x) {
    return layout.map_x + x * layout.draw_w / MAP_W;
}

int tile_right(MapLayout layout, int x) {
    return layout.map_x + (x + 1) * layout.draw_w / MAP_W;
}

int tile_top(MapLayout layout, int y) {
    return layout.map_y + y * layout.draw_h / MAP_H;
}

int tile_bottom(MapLayout layout, int y) {
    return layout.map_y + (y + 1) * layout.draw_h / MAP_H;
}
