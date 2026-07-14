#include "render/country_identity_block.h"

#include "render/render_common.h"

#include <stdio.h>
#include <string.h>

static void copy_name_prefix(char out[49], const char *name) {
    size_t count;
    if (!name) {
        out[0] = '\0';
        return;
    }
    count = strlen(name);
    if (count > 48) count = 48;
    while (count > 0 &&
           (((unsigned char)name[count] & 0xc0u) == 0x80u)) {
        count--;
    }
    memcpy(out, name, count);
    out[count] = '\0';
}

RECT country_identity_block_draw(HDC hdc, RECT bounds, char symbol,
                                 const char *localized_name, COLORREF color) {
    char name[49];
    char text[53];
    SIZE size;
    RECT block = {bounds.left, bounds.top, bounds.left, bounds.top};

    if (!hdc || bounds.left >= bounds.right - 10) return block;
    copy_name_prefix(name, localized_name);
    snprintf(text, sizeof(text), "%c %s", symbol, name);
    measure_text_utf8(hdc, text, &size);
    block.right = min(bounds.left + size.cx + 20, bounds.right);
    block.bottom = min(bounds.top + 22, bounds.bottom);
    if (block.right <= block.left + 10 || block.bottom <= block.top) {
        block.right = block.left;
        block.bottom = block.top;
        return block;
    }
    fill_rect_alpha(hdc, block, color, 112);
    draw_text_rect(hdc, (RECT){block.left + 6, block.top,
                               block.right - 6, block.bottom},
                   text, RGB(246, 248, 250),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    return block;
}
