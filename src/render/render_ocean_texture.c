#include "render/render_ocean_texture.h"

#include "render/render_layer_cache.h"
#include "render/render_ocean_assets.h"

static LayerCache ocean_texture_cache;
static int ocean_texture_score;

static COLORREF ocean_texture_base_color(void) {
    return RGB(55, 135, 199);
}

static void fill_rect_color(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

unsigned int render_ocean_texture_key(RECT client, RECT viewport) {
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, client.right - client.left);
    key = render_layer_mix_key(key, client.bottom - client.top);
    key = render_layer_mix_key(key, viewport.left);
    key = render_layer_mix_key(key, viewport.top);
    key = render_layer_mix_key(key, viewport.right);
    key = render_layer_mix_key(key, viewport.bottom);
    return render_layer_mix_key(key, ocean_assets_texture_ready());
}

static int texture_cache_matches(RECT client, unsigned int key) {
    return ocean_texture_cache.valid && ocean_texture_cache.key == key &&
           ocean_texture_cache.width == client.right - client.left &&
           ocean_texture_cache.height == client.bottom - client.top;
}

int render_ocean_texture_ensure(HDC hdc, RECT client, MapLayout layout, RECT viewport) {
    unsigned int key = render_ocean_texture_key(client, viewport);
    if (texture_cache_matches(client, key)) return 1;
    if (!render_layer_cache_ensure(hdc, &ocean_texture_cache, client, layout, 0, 0)) return 0;
    fill_rect_color(ocean_texture_cache.dc, viewport, ocean_texture_base_color());
    if (ocean_assets_draw_texture(ocean_texture_cache.dc, viewport)) {
        ocean_texture_score = 900;
    } else {
        ocean_texture_score = 330;
    }
    ocean_texture_cache.key = key;
    ocean_texture_cache.valid = 1;
    return 1;
}

int render_ocean_texture_copy(HDC hdc, RECT viewport) {
    if (!ocean_texture_cache.valid || !ocean_texture_cache.dc) return 0;
    return BitBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
                  viewport.bottom - viewport.top, ocean_texture_cache.dc,
                  viewport.left, viewport.top, SRCCOPY);
}

int render_ocean_texture_score(void) {
    return ocean_texture_score;
}

void render_ocean_texture_reset_debug(void) {
    ocean_texture_cache.valid = 0;
    ocean_texture_score = 0;
}
