#ifndef WORLD_SIM_MAP_LABEL_STYLE_H
#define WORLD_SIM_MAP_LABEL_STYLE_H

#include "render/render.h"

typedef enum {
    LABEL_COUNTRY = 0,
    LABEL_CAPITAL,
    LABEL_MAJOR_CITY,
    LABEL_CITY,
    LABEL_PROVINCE,
    LABEL_PORT,
    LABEL_SELECTED,
    LABEL_KIND_COUNT
} MapLabelKind;

typedef struct {
    int font_size;
    int weight;
    int italic;
    const wchar_t *font_zh;
    const wchar_t *font_en;
    COLORREF text_color;
    COLORREF outline_color;
    COLORREF shadow_color;
    int outline_px;
    int shadow_px;
    int min_tile_size;
    int priority;
    int centered;
} MapLabelStyle;

MapLabelStyle map_label_style_for(MapLabelKind kind, int tile_size, int large, int selected);
void map_label_measure(HDC hdc, const MapLabelStyle *style, const char *text, SIZE *out);
void map_label_draw(HDC hdc, const MapLabelStyle *style, int x, int y, const char *text);

#endif
