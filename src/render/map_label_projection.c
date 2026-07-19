#include "render/map_label_projection.h"

static int rects_overlap(RECT a, RECT b) {
    return a.left < b.right && a.right > b.left &&
           a.top < b.bottom && a.bottom > b.top;
}

int map_label_projection_anchor_x(MapLayout layout, int map_w, int anchor_x2) {
    return layout.map_x + anchor_x2 * layout.draw_w / max(1, map_w * 2);
}

int map_label_projection_anchor_y(MapLayout layout, int map_h, int anchor_y2) {
    return layout.map_y + anchor_y2 * layout.draw_h / max(1, map_h * 2);
}

RECT map_label_projection_rect(int x, int y, SIZE size, int pad) {
    RECT rect = {x - pad, y - pad, x + size.cx + pad, y + size.cy + pad};
    return rect;
}

int map_label_projection_rect_visible(RECT rect, RECT viewport) {
    return rects_overlap(rect, viewport);
}

int map_label_projection_slot_open(const RECT *used, int used_count, RECT candidate) {
    int i;
    for (i = 0; i < used_count; i++) {
        if (rects_overlap(used[i], candidate)) return 0;
    }
    return 1;
}

void map_label_projection_slot_position(int centered, SIZE size, int anchor_x,
                                        int anchor_y, int slot, int *x, int *y) {
    if (centered) {
        *x = anchor_x - size.cx / 2;
        *y = anchor_y - size.cy / 2;
    } else if (slot == 0) {
        *x = anchor_x + 12;
        *y = anchor_y - size.cy / 2;
    } else if (slot == 1) {
        *x = anchor_x - size.cx - 12;
        *y = anchor_y - size.cy / 2;
    } else if (slot == 2) {
        *x = anchor_x - size.cx / 2;
        *y = anchor_y - size.cy - 13;
    } else if (slot == 3) {
        *x = anchor_x - size.cx / 2;
        *y = anchor_y + 13;
    } else {
        *x = anchor_x + 10;
        *y = anchor_y + 8;
    }
}
