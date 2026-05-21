#include "platform/sdl_render.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "data/province_names.h"
#include "platform/sdl_render_ui.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "ui/ui_l10n.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

#include <string.h>

static Color32 color_blend(Color32 base, Color32 overlay, int percent) {
    int r = GetRValue(base) * (100 - percent) / 100 + GetRValue(overlay) * percent / 100;
    int g = GetGValue(base) * (100 - percent) / 100 + GetGValue(overlay) * percent / 100;
    int b = GetBValue(base) * (100 - percent) / 100 + GetBValue(overlay) * percent / 100;
    return RGB(r, g, b);
}

static Uint32 pixel_from_color(Color32 color) {
    return 0xff000000u | ((Uint32)GetRValue(color) << 16) |
           ((Uint32)GetGValue(color) << 8) | (Uint32)GetBValue(color);
}

static Color32 geography_color_sdl(Geography geography) {
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
        default: return RGB(0, 0, 0);
    }
}

static Color32 climate_color_sdl(Climate climate) {
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
        default: return RGB(0, 0, 0);
    }
}

static Color32 region_color_sdl(int region_id) {
    unsigned int h = (unsigned int)(region_id + 1) * 2654435761u;
    return RGB(96 + (h & 63), 104 + ((h >> 8) & 63), 112 + ((h >> 16) & 63));
}

static int snapshot_tile_is_land(const SnapshotTile *tile) {
    return tile && tile->geography != GEO_OCEAN && tile->geography != GEO_COAST &&
           tile->geography != GEO_LAKE && tile->geography != GEO_BAY;
}

static Color32 overview_color_sdl(const SnapshotTile *tile) {
    Color32 base = geography_color_sdl((Geography)tile->geography);
    Color32 climate = climate_color_sdl((Climate)tile->climate);
    int blend = snapshot_tile_is_land(tile) ? 48 : 18;
    Color32 color = color_blend(base, climate, blend);
    int elev = tile->elevation;

    if (!snapshot_tile_is_land(tile)) return color;
    if (elev > 55) return color_blend(color, RGB(38, 35, 32), clamp((elev - 55) / 3, 0, 18));
    return color_blend(color, RGB(236, 230, 198), clamp((55 - elev) / 4, 0, 12));
}

static Color32 route_potential_color_sdl(const SnapshotTile *tile) {
    if (!tile) return RGB(0, 0, 0);
    if (snapshot_tile_is_land(tile)) {
        if (tile->region_id >= 0) return color_blend(region_color_sdl(tile->region_id), RGB(35, 42, 38), 54);
        return color_blend(overview_color_sdl(tile), RGB(35, 42, 38), 46);
    }
    if (tile->water_deep_percent > 62 || tile->water_depth > 150) return RGB(38, 92, 154);
    if (tile->water_deep_percent > 28 || tile->water_depth > 92) return RGB(54, 122, 184);
    return RGB(92, 177, 214);
}

static Color32 tile_color(const RenderSnapshot *snapshot, int x, int y, int mode) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);

    if (!tile) return RGB(0, 0, 0);
    if (mode == DISPLAY_POLITICAL && tile->owner >= 0 &&
        tile->owner < snapshot->civ_count && snapshot->civs[tile->owner].alive) {
        return snapshot->civs[tile->owner].color;
    }
    if (mode == DISPLAY_GEOGRAPHY) return geography_color_sdl((Geography)tile->geography);
    if (mode == DISPLAY_CLIMATE) return climate_color_sdl((Climate)tile->climate);
    if (mode == DISPLAY_REGIONS && tile->region_id >= 0) return region_color_sdl(tile->region_id);
    if (mode == DISPLAY_ROUTE_POTENTIAL) return route_potential_color_sdl(tile);
    return overview_color_sdl(tile);
}

static int owner_alive(const RenderSnapshot *snapshot, int owner) {
    return snapshot && owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive;
}

static int map_neighbor_edge(const RenderSnapshot *snapshot, int x, int y, int nx, int ny, int mode) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
    const SnapshotTile *next = render_snapshot_tile_at(snapshot, nx, ny);
    if (!tile || !next) return 0;
    if (snapshot_tile_is_land(tile) != snapshot_tile_is_land(next)) return 2;
    if (mode == DISPLAY_POLITICAL && owner_alive(snapshot, tile->owner) &&
        owner_alive(snapshot, next->owner) && tile->owner != next->owner) return 3;
    if ((mode == DISPLAY_REGIONS || mode == DISPLAY_ROUTE_POTENTIAL) &&
        tile->region_id >= 0 && next->region_id >= 0 && tile->region_id != next->region_id) return 1;
    return 0;
}

static Color32 overlay_pixel_color(const RenderSnapshot *snapshot, int x, int y, int mode, Color32 base) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
    int edge = 0;
    if (!tile) return base;
    if (tile->river) base = color_blend(base, RGB(64, 130, 190), snapshot_tile_is_land(tile) ? 60 : 28);
    if (x + 1 < snapshot->map_w) edge = max(edge, map_neighbor_edge(snapshot, x, y, x + 1, y, mode));
    if (y + 1 < snapshot->map_h) edge = max(edge, map_neighbor_edge(snapshot, x, y, x, y + 1, mode));
    if (edge == 3) return color_blend(base, RGB(35, 28, 22), 72);
    if (edge == 2) return color_blend(base, RGB(230, 219, 178), 44);
    if (edge == 1) return color_blend(base, RGB(55, 50, 44), 36);
    return base;
}

void sdl_render_init(SdlMapRenderer *map) {
    if (!map) return;
    memset(map, 0, sizeof(*map));
    map->dirty = 1;
}

void sdl_render_shutdown(SdlMapRenderer *map) {
    if (!map) return;
    if (map->map_texture) SDL_DestroyTexture(map->map_texture);
    memset(map, 0, sizeof(*map));
}

void sdl_render_invalidate_map(SdlMapRenderer *map) {
    if (map) map->dirty = 1;
}

static int ensure_map_texture(SdlMapRenderer *map, SDL_Renderer *renderer,
                              const RenderSnapshot *snapshot, int mode) {
    void *pixels;
    int pitch;
    int x;
    int y;

    if (!map || !renderer || !snapshot || snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    if (!map->map_texture || map->texture_w != snapshot->map_w || map->texture_h != snapshot->map_h) {
        if (map->map_texture) SDL_DestroyTexture(map->map_texture);
        map->map_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, snapshot->map_w, snapshot->map_h);
        if (!map->map_texture) return 0;
        SDL_SetTextureScaleMode(map->map_texture, SDL_SCALEMODE_NEAREST);
        map->texture_w = snapshot->map_w;
        map->texture_h = snapshot->map_h;
        map->dirty = 1;
    }
    if (!map->dirty && map->texture_mode == mode &&
        map->texture_revision == snapshot->revision) return 1;
    if (!SDL_LockTexture(map->map_texture, NULL, &pixels, &pitch)) return 0;
    for (y = 0; y < snapshot->map_h; y++) {
        Uint32 *row = (Uint32 *)((unsigned char *)pixels + y * pitch);
        for (x = 0; x < snapshot->map_w; x++) {
            Color32 base = tile_color(snapshot, x, y, mode);
            row[x] = pixel_from_color(overlay_pixel_color(snapshot, x, y, mode, base));
        }
    }
    SDL_UnlockTexture(map->map_texture);
    map->texture_mode = mode;
    map->texture_revision = snapshot->revision;
    map->dirty = 0;
    return 1;
}

static void draw_plague_overlay(SDL_Renderer *renderer, const MapLayout *layout,
                                const RenderSnapshot *snapshot) {
    int i;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        int severity = i < MAX_CITIES ? snapshot->plague_city_severity[i] : 0;
        int size = clamp(8 + severity / 8, 8, 46);
        SDL_FRect r;
        if (!city->alive || severity <= 0) continue;
        r.x = (float)layout->map_x + (float)city->x * (float)layout->draw_w / (float)max(1, snapshot->map_w) - (float)size * 0.5f;
        r.y = (float)layout->map_y + (float)city->y * (float)layout->draw_h / (float)max(1, snapshot->map_h) - (float)size * 0.5f;
        r.w = (float)size;
        r.h = (float)size;
        SDL_SetRenderDrawColor(renderer, 13, 80, 45, (Uint8)clamp(70 + severity / 2, 90, 190));
        SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_cities(SDL_Renderer *renderer, SdlText *text, const MapLayout *layout,
                        const RenderSnapshot *snapshot) {
    int i;
    float label_threshold = map_zoom_percent >= 125 ? 1.0f : 0.0f;

    for (i = 0; i < snapshot->city_count; i++) {
        SDL_FRect r;
        const SnapshotCity *city = &snapshot->cities[i];
        float cx;
        float cy;
        if (!city->alive) continue;
        cx = (float)layout->map_x + (float)city->x * (float)layout->draw_w / (float)max(1, snapshot->map_w);
        cy = (float)layout->map_y + (float)city->y * (float)layout->draw_h / (float)max(1, snapshot->map_h);
        r.x = cx - 2.0f;
        r.y = cy - 2.0f;
        r.w = city->capital ? 6.0f : 4.0f;
        r.h = city->capital ? 6.0f : 4.0f;
        SDL_SetRenderDrawColor(renderer, city->port ? 145 : 245, city->port ? 205 : 243,
                               city->port ? 245 : 220, 255);
        SDL_RenderFillRect(renderer, &r);
        if (city->capital || label_threshold > 0.0f) {
            const char *name = city->name[0] ? city->name : ui_l10n("City", "城市");
            sdl_text_draw(renderer, text->small, (int)cx + 6, (int)cy - 8,
                          (SDL_Color){245, 241, 214, 255}, name);
        }
    }
}

static void draw_routes(SDL_Renderer *renderer, const MapLayout *layout,
                        const RenderSnapshot *snapshot) {
    int i;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (i = 0; i < snapshot->lane_count; i++) {
        int p;
        const SnapshotSeaLane *route = &snapshot->lanes[i];
        if (!route->active) continue;
        if (route->type == SEA_LANE_DEEP) SDL_SetRenderDrawColor(renderer, 146, 190, 255, 170);
        else SDL_SetRenderDrawColor(renderer, 224, 238, 255, 155);
        for (p = 1; p < route->point_count; p++) {
            float x1 = (float)layout->map_x + (float)route->points[p - 1].x * (float)layout->draw_w / (float)max(1, snapshot->map_w);
            float y1 = (float)layout->map_y + (float)route->points[p - 1].y * (float)layout->draw_h / (float)max(1, snapshot->map_h);
            float x2 = (float)layout->map_x + (float)route->points[p].x * (float)layout->draw_w / (float)max(1, snapshot->map_w);
            float y2 = (float)layout->map_y + (float)route->points[p].y * (float)layout->draw_h / (float)max(1, snapshot->map_h);
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static int valid_alive_civ_snapshot(const RenderSnapshot *snapshot, int civ_id) {
    return snapshot && civ_id >= 0 && civ_id < snapshot->civ_count && snapshot->civs[civ_id].alive;
}

static int screen_x_for_tile(const MapLayout *layout, const RenderSnapshot *snapshot, int x) {
    return layout->map_x + x * layout->draw_w / max(1, snapshot->map_w);
}

static int screen_y_for_tile(const MapLayout *layout, const RenderSnapshot *snapshot, int y) {
    return layout->map_y + y * layout->draw_h / max(1, snapshot->map_h);
}

static int tile_owner_snapshot(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
    return tile ? tile->owner : -1;
}

static int visible_tile_bounds(const RenderSnapshot *snapshot, const MapLayout *layout,
                               RECT client, int *min_x, int *max_x, int *min_y, int *max_y) {
    int left = max(client.left, layout->map_x);
    int top = max(TOP_BAR_H, layout->map_y);
    int right = min(client.right - side_panel_w, layout->map_x + layout->draw_w);
    int bottom = min(client.bottom - BOTTOM_BAR_H, layout->map_y + layout->draw_h);
    if (!snapshot || layout->draw_w <= 0 || layout->draw_h <= 0 || right <= left || bottom <= top) return 0;
    *min_x = clamp((left - layout->map_x) * snapshot->map_w / layout->draw_w - 1, 0, snapshot->map_w - 1);
    *max_x = clamp((right - layout->map_x) * snapshot->map_w / layout->draw_w + 1, 0, snapshot->map_w - 1);
    *min_y = clamp((top - layout->map_y) * snapshot->map_h / layout->draw_h - 1, 0, snapshot->map_h - 1);
    *max_y = clamp((bottom - layout->map_y) * snapshot->map_h / layout->draw_h + 1, 0, snapshot->map_h - 1);
    return 1;
}

static SDL_Color relation_highlight_color(const RenderSnapshot *snapshot, int civ_id) {
    if (!valid_alive_civ_snapshot(snapshot, selected_civ) || civ_id == selected_civ) {
        return (SDL_Color){255, 245, 166, 110};
    }
    if (snapshot->relations[selected_civ][civ_id].state == DIPLOMACY_WAR ||
        snapshot->wars[selected_civ][civ_id].active) return (SDL_Color){214, 70, 58, 100};
    if (snapshot->civs[civ_id].overlord == selected_civ) return (SDL_Color){232, 200, 86, 96};
    if (snapshot->civs[selected_civ].overlord == civ_id) return (SDL_Color){176, 112, 218, 96};
    return (SDL_Color){255, 245, 166, 70};
}

static void draw_country_overlay_one(SDL_Renderer *renderer, const RenderSnapshot *snapshot,
                                     const MapLayout *layout, RECT client, int civ_id, int strong) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    SDL_Color fill_color = relation_highlight_color(snapshot, civ_id);
    SDL_Color edge_color = fill_color;
    if (!valid_alive_civ_snapshot(snapshot, civ_id)) return;
    if (!visible_tile_bounds(snapshot, layout, client, &min_x, &max_x, &min_y, &max_y)) return;
    fill_color.a = strong ? 80 : 44;
    edge_color.a = strong ? 220 : 150;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            SDL_FRect rect;
            if (tile_owner_snapshot(snapshot, x, y) != civ_id) continue;
            rect.x = (float)screen_x_for_tile(layout, snapshot, x);
            rect.y = (float)screen_y_for_tile(layout, snapshot, y);
            rect.w = (float)max(1, screen_x_for_tile(layout, snapshot, x + 1) - (int)rect.x);
            rect.h = (float)max(1, screen_y_for_tile(layout, snapshot, y + 1) - (int)rect.y);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_SetRenderDrawColor(renderer, edge_color.r, edge_color.g, edge_color.b, edge_color.a);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int l;
            int r;
            int t;
            int b;
            if (tile_owner_snapshot(snapshot, x, y) != civ_id) continue;
            l = screen_x_for_tile(layout, snapshot, x);
            r = screen_x_for_tile(layout, snapshot, x + 1);
            t = screen_y_for_tile(layout, snapshot, y);
            b = screen_y_for_tile(layout, snapshot, y + 1);
            if (tile_owner_snapshot(snapshot, x, y - 1) != civ_id) SDL_RenderLine(renderer, (float)l, (float)t, (float)r, (float)t);
            if (tile_owner_snapshot(snapshot, x, y + 1) != civ_id) SDL_RenderLine(renderer, (float)l, (float)b, (float)r, (float)b);
            if (tile_owner_snapshot(snapshot, x - 1, y) != civ_id) SDL_RenderLine(renderer, (float)l, (float)t, (float)l, (float)b);
            if (tile_owner_snapshot(snapshot, x + 1, y) != civ_id) SDL_RenderLine(renderer, (float)r, (float)t, (float)r, (float)b);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_selected_country_overlay(SDL_Renderer *renderer, const RenderSnapshot *snapshot,
                                          const MapLayout *layout, RECT client) {
    int i;
    int overlord;
    if (!valid_alive_civ_snapshot(snapshot, selected_civ)) return;
    overlord = snapshot->civs[selected_civ].overlord;
    if (valid_alive_civ_snapshot(snapshot, overlord)) draw_country_overlay_one(renderer, snapshot, layout, client, overlord, 0);
    for (i = 0; i < snapshot->civ_count; i++) {
        if (i == selected_civ || !snapshot->civs[i].alive) continue;
        if (snapshot->civs[i].overlord == selected_civ ||
            snapshot->relations[selected_civ][i].state == DIPLOMACY_WAR ||
            snapshot->wars[selected_civ][i].active) {
            draw_country_overlay_one(renderer, snapshot, layout, client, i, 0);
        }
    }
    draw_country_overlay_one(renderer, snapshot, layout, client, selected_civ, 1);
}

static int civ_screen_focus(const RenderSnapshot *snapshot, const MapLayout *layout,
                            int civ_id, float *out_x, float *out_y) {
    int city_id;
    const SnapshotCity *city;
    if (!valid_alive_civ_snapshot(snapshot, civ_id)) return 0;
    city_id = snapshot->civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= snapshot->city_count) return 0;
    city = &snapshot->cities[city_id];
    if (!city->alive) return 0;
    *out_x = (float)layout->map_x + (float)city->x * (float)layout->draw_w / (float)max(1, snapshot->map_w);
    *out_y = (float)layout->map_y + (float)city->y * (float)layout->draw_h / (float)max(1, snapshot->map_h);
    return 1;
}

static void draw_wide_line(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, int width) {
    int offset;
    for (offset = -width / 2; offset <= width / 2; offset++) {
        SDL_RenderLine(renderer, x1 + (float)offset, y1, x2 + (float)offset, y2);
        SDL_RenderLine(renderer, x1, y1 + (float)offset, x2, y2 + (float)offset);
    }
}

static void draw_relation_lines(SDL_Renderer *renderer, const RenderSnapshot *snapshot,
                                const MapLayout *layout) {
    int i;
    int j;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (i = 0; i < snapshot->civ_count; i++) {
        for (j = i + 1; j < snapshot->civ_count; j++) {
            int war;
            int vassal;
            int strong;
            float x1;
            float y1;
            float x2;
            float y2;
            if (!snapshot->civs[i].alive || !snapshot->civs[j].alive) continue;
            war = snapshot->relations[i][j].state == DIPLOMACY_WAR || snapshot->wars[i][j].active;
            vassal = snapshot->civs[i].overlord == j || snapshot->civs[j].overlord == i;
            if (!war && !vassal) continue;
            if (!civ_screen_focus(snapshot, layout, i, &x1, &y1) ||
                !civ_screen_focus(snapshot, layout, j, &x2, &y2)) continue;
            strong = (i == selected_civ || j == selected_civ);
            if (war) SDL_SetRenderDrawColor(renderer, 224, 58, 48, strong ? 220 : 120);
            else SDL_SetRenderDrawColor(renderer, 232, 200, 86, strong ? 210 : 110);
            draw_wide_line(renderer, x1, y1, x2, y2, strong ? 3 : 2);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static int sdl_render_draw_internal(SdlMapRenderer *map, SDL_Renderer *renderer, SdlText *text,
                                    int window_w, int window_h, int mode, int running,
                                    int months_per_tick, int present) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    RECT client = {0, 0, window_w, window_h};
    MapLayout layout;
    SDL_FRect dst;

    if (!snapshot) return 0;
    (void)mode;
    (void)running;
    (void)months_per_tick;
    layout = get_map_layout(client);
    dst.x = (float)layout.map_x;
    dst.y = (float)layout.map_y;
    dst.w = (float)layout.draw_w;
    dst.h = (float)layout.draw_h;

    SDL_SetRenderDrawColor(renderer, 15, 20, 25, 255);
    SDL_RenderClear(renderer);
    if (!ensure_map_texture(map, renderer, snapshot, mode)) {
        render_snapshot_release(snapshot);
        return 0;
    }
    SDL_RenderTexture(renderer, map->map_texture, NULL, &dst);
    draw_routes(renderer, &layout, snapshot);
    draw_selected_country_overlay(renderer, snapshot, &layout, client);
    draw_relation_lines(renderer, snapshot, &layout);
    draw_plague_overlay(renderer, &layout, snapshot);
    draw_cities(renderer, text, &layout, snapshot);
    if (selected_x >= 0 && selected_y >= 0) {
        SDL_FRect sel = {
            (float)layout.map_x + (float)selected_x * (float)layout.draw_w / (float)max(1, snapshot->map_w),
            (float)layout.map_y + (float)selected_y * (float)layout.draw_h / (float)max(1, snapshot->map_h),
            max(3.0f, (float)layout.draw_w / (float)max(1, snapshot->map_w)),
            max(3.0f, (float)layout.draw_h / (float)max(1, snapshot->map_h))
        };
        SDL_SetRenderDrawColor(renderer, 255, 245, 166, 255);
        SDL_RenderRect(renderer, &sel);
    }
    render_context_begin(snapshot);
    sdl_render_draw_ui(renderer, text, snapshot, client);
    render_context_end();
    if (present) SDL_RenderPresent(renderer);
    render_snapshot_release(snapshot);
    return 1;
}

int sdl_render_draw(SdlMapRenderer *map, SDL_Renderer *renderer, SdlText *text,
                    int window_w, int window_h, int mode, int running, int months_per_tick) {
    return sdl_render_draw_internal(map, renderer, text, window_w, window_h, mode,
                                    running, months_per_tick, 1);
}

int sdl_render_draw_backbuffer(SdlMapRenderer *map, SDL_Renderer *renderer, SdlText *text,
                               int window_w, int window_h, int mode, int running,
                               int months_per_tick) {
    return sdl_render_draw_internal(map, renderer, text, window_w, window_h, mode,
                                    running, months_per_tick, 0);
}
