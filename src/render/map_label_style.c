#include "render/map_label_style.h"

#include "core/game_types.h"
#include "ui/ui_types.h"

#include <stdlib.h>
#include <string.h>

#define LABEL_FONT_CACHE_MAX 32

typedef struct {
    int valid;
    int size;
    int weight;
    int italic;
    int language;
    const wchar_t *face;
    HFONT font;
} LabelFontCache;

static LabelFontCache font_cache[LABEL_FONT_CACHE_MAX];

static int clamp_int_local(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static const wchar_t *style_face(const MapLabelStyle *style) {
    return ui_language == UI_LANG_ZH ? style->font_zh : style->font_en;
}

static HFONT cached_font(const MapLabelStyle *style) {
    int i;
    const wchar_t *face = style_face(style);
    for (i = 0; i < LABEL_FONT_CACHE_MAX; i++) {
        LabelFontCache *entry = &font_cache[i];
        if (!entry->valid) continue;
        if (entry->size == style->font_size && entry->weight == style->weight &&
            entry->italic == style->italic && entry->language == ui_language &&
            entry->face == face) {
            return entry->font;
        }
    }
    for (i = 0; i < LABEL_FONT_CACHE_MAX; i++) {
        LabelFontCache *entry = &font_cache[i];
        if (entry->valid) continue;
        entry->valid = 1;
        entry->size = style->font_size;
        entry->weight = style->weight;
        entry->italic = style->italic;
        entry->language = ui_language;
        entry->face = face;
        entry->font = CreateFontW(style->font_size, 0, 0, 0, style->weight,
                                  (DWORD)style->italic, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH, face);
        return entry->font;
    }
    return GetStockObject(DEFAULT_GUI_FONT);
}

static int utf8_to_wide(const char *text, wchar_t *wide, int wide_count) {
    int len;
    if (!text || !wide || wide_count <= 0) return 0;
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_count);
    if (len <= 0) {
        wide[0] = 0;
        return 0;
    }
    return len - 1;
}

MapLabelStyle map_label_style_for(MapLabelKind kind, int tile_size, int large, int selected) {
    MapLabelStyle style;
    memset(&style, 0, sizeof(style));
    style.font_zh = L"Microsoft YaHei UI";
    style.font_en = L"Segoe UI";
    style.weight = FW_NORMAL;
    style.shadow_px = 1;
    style.shadow_color = RGB(24, 24, 24);
    style.min_tile_size = 1;
    style.priority = 100;
    switch (kind) {
        case LABEL_COUNTRY:
            style.font_zh = L"Microsoft YaHei UI";
            style.font_en = L"Georgia";
            style.font_size = clamp_int_local(20 + tile_size / 2 + (large ? 4 : 0), 20, 32);
            style.weight = FW_BOLD;
            style.text_color = RGB(245, 225, 170);
            style.outline_color = RGB(38, 32, 24);
            style.shadow_color = RGB(24, 20, 16);
            style.outline_px = 2;
            style.priority = 800;
            style.centered = 1;
            break;
        case LABEL_CAPITAL:
            style.font_size = clamp_int_local(17 + tile_size / 8, 17, 19);
            style.weight = FW_SEMIBOLD;
            style.text_color = RGB(255, 238, 190);
            style.shadow_color = RGB(24, 24, 20);
            style.min_tile_size = 1;
            style.priority = 650;
            break;
        case LABEL_MAJOR_CITY:
            style.font_size = clamp_int_local(14 + tile_size / 7, 14, 16);
            style.text_color = RGB(238, 232, 210);
            style.min_tile_size = 2;
            style.priority = 500;
            break;
        case LABEL_CITY:
            style.font_size = tile_size >= 8 ? 14 : 13;
            style.text_color = RGB(220, 218, 205);
            style.min_tile_size = 3;
            style.priority = 320;
            break;
        case LABEL_PROVINCE:
            style.font_zh = L"DengXian";
            style.font_size = clamp_int_local(15 + tile_size / 7, 15, 17);
            style.italic = ui_language == UI_LANG_ZH ? 0 : 1;
            style.text_color = RGB(204, 198, 174);
            style.shadow_color = RGB(24, 24, 22);
            style.min_tile_size = 3;
            style.priority = 380;
            style.centered = 1;
            break;
        case LABEL_PORT:
            style.font_size = tile_size >= 8 ? 14 : 13;
            style.text_color = RGB(214, 238, 246);
            style.shadow_color = RGB(18, 34, 40);
            style.min_tile_size = 2;
            style.priority = 460;
            break;
        case LABEL_SELECTED:
            style.font_size = 16;
            style.weight = FW_BOLD;
            style.text_color = RGB(255, 248, 210);
            style.outline_color = RGB(45, 36, 20);
            style.shadow_color = RGB(24, 20, 16);
            style.outline_px = 1;
            style.priority = 1000;
            break;
        default:
            style.font_size = 12;
            style.text_color = RGB(220, 220, 210);
            break;
    }
    if (selected) {
        style.text_color = RGB(255, 248, 214);
        style.outline_px = max(style.outline_px, 1);
        style.priority = 1000;
    }
    return style;
}

void map_label_measure(HDC hdc, const MapLabelStyle *style, const char *text, SIZE *out) {
    wchar_t wide[160];
    RECT rect = {0, 0, 1200, 80};
    HFONT old_font;
    out->cx = 0;
    out->cy = 0;
    if (!utf8_to_wide(text, wide, 160)) return;
    old_font = SelectObject(hdc, cached_font(style));
    DrawTextW(hdc, wide, -1, &rect, DT_SINGLELINE | DT_CALCRECT);
    SelectObject(hdc, old_font);
    out->cx = rect.right - rect.left;
    out->cy = rect.bottom - rect.top;
}

void map_label_draw(HDC hdc, const MapLabelStyle *style, int x, int y, const char *text) {
    wchar_t wide[160];
    HFONT old_font;
    int dx, dy;
    if (!utf8_to_wide(text, wide, 160)) return;
    old_font = SelectObject(hdc, cached_font(style));
    SetBkMode(hdc, TRANSPARENT);
    if (style->shadow_px > 0) {
        SetTextColor(hdc, style->shadow_color);
        TextOutW(hdc, x + style->shadow_px, y + style->shadow_px, wide, lstrlenW(wide));
    }
    if (style->outline_px > 0) {
        SetTextColor(hdc, style->outline_color);
        for (dy = -style->outline_px; dy <= style->outline_px; dy++) {
            for (dx = -style->outline_px; dx <= style->outline_px; dx++) {
                if (dx == 0 && dy == 0) continue;
                if (abs(dx) + abs(dy) > style->outline_px + 1) continue;
                TextOutW(hdc, x + dx, y + dy, wide, lstrlenW(wide));
            }
        }
    }
    SetTextColor(hdc, style->text_color);
    TextOutW(hdc, x, y, wide, lstrlenW(wide));
    SelectObject(hdc, old_font);
}
