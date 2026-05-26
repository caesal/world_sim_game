#include "map_labels.h"

#include "render/map_label_cache.h"
#include "render/render_context.h"

void draw_map_labels(HDC hdc, RECT client, MapLayout layout) {
    int saved_dc = SaveDC(hdc);
    RECT viewport = get_map_content_rect(client);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(hdc, TRANSPARENT);
    map_label_cache_draw_labels(hdc, client, layout, render_context_snapshot());
    RestoreDC(hdc, saved_dc);
}
