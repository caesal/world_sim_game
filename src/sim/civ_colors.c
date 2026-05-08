#include "sim/civ_colors.h"

#include "core/game_types.h"
#include "sim/regions.h"

static const Color32 POLITICAL_PALETTE[] = {
    COLOR32_RGB(214, 48, 72), COLOR32_RGB(88, 92, 230),
    COLOR32_RGB(232, 166, 36), COLOR32_RGB(178, 72, 218),
    COLOR32_RGB(226, 90, 42), COLOR32_RGB(32, 170, 190),
    COLOR32_RGB(220, 58, 138), COLOR32_RGB(116, 82, 220),
    COLOR32_RGB(224, 126, 44), COLOR32_RGB(196, 52, 96),
    COLOR32_RGB(72, 112, 224), COLOR32_RGB(194, 74, 58),
    COLOR32_RGB(154, 76, 218), COLOR32_RGB(218, 194, 48),
    COLOR32_RGB(52, 166, 148), COLOR32_RGB(232, 84, 100),
    COLOR32_RGB(108, 132, 232), COLOR32_RGB(218, 112, 58),
    COLOR32_RGB(190, 66, 170), COLOR32_RGB(84, 154, 210),
    COLOR32_RGB(238, 144, 56), COLOR32_RGB(166, 92, 226),
    COLOR32_RGB(226, 64, 120), COLOR32_RGB(68, 142, 188)
};

static const Color32 BACKGROUND_COLORS[] = {
    COLOR32_RGB(80, 145, 78), COLOR32_RGB(96, 168, 94),
    COLOR32_RGB(74, 132, 174), COLOR32_RGB(80, 150, 190),
    COLOR32_RGB(122, 112, 90), COLOR32_RGB(132, 128, 116),
    COLOR32_RGB(92, 82, 62)
};

static int channel_r(Color32 color) { return (int)(color & 255); }
static int channel_g(Color32 color) { return (int)((color >> 8) & 255); }
static int channel_b(Color32 color) { return (int)((color >> 16) & 255); }

static int color_distance(Color32 a, Color32 b) {
    int dr = channel_r(a) - channel_r(b);
    int dg = channel_g(a) - channel_g(b);
    int db = channel_b(a) - channel_b(b);
    return dr * dr + dg * dg + db * db;
}

static int min_background_distance(Color32 color) {
    int i;
    int best = 100000000;
    for (i = 0; i < (int)(sizeof(BACKGROUND_COLORS) / sizeof(BACKGROUND_COLORS[0])); i++) {
        int dist = color_distance(color, BACKGROUND_COLORS[i]);
        if (dist < best) best = dist;
    }
    return best;
}

static int neighbor_color_distance(Color32 color, int seed_region) {
    const NaturalRegion *region = regions_get(seed_region);
    int best = 100000000;
    int i;

    if (!region) return best;
    for (i = 0; i < region->neighbor_count; i++) {
        int neighbor = region->neighbors[i];
        int owner;
        if (neighbor < 0 || neighbor >= region_count) continue;
        owner = natural_regions[neighbor].owner_civ;
        if (owner >= 0 && owner < civ_count && civs[owner].alive) {
            int dist = color_distance(color, civs[owner].color);
            if (dist < best) best = dist;
        }
    }
    return best;
}

static int global_color_distance(Color32 color, int civ_id) {
    int best = 100000000;
    int i;
    for (i = 0; i < civ_count; i++) {
        int dist;
        if (i == civ_id || !civs[i].alive) continue;
        dist = color_distance(color, civs[i].color);
        if (dist < best) best = dist;
    }
    return best;
}

Color32 civilization_pick_auto_color(int civ_id, int seed_region) {
    int i;
    int best_score = -1;
    Color32 best = POLITICAL_PALETTE[0];
    int palette_count = (int)(sizeof(POLITICAL_PALETTE) / sizeof(POLITICAL_PALETTE[0]));

    for (i = 0; i < palette_count; i++) {
        int index = (i + civ_id * 7) % palette_count;
        Color32 color = POLITICAL_PALETTE[index];
        int background = min_background_distance(color);
        int global = global_color_distance(color, civ_id);
        int neighbor = neighbor_color_distance(color, seed_region);
        int score = background / 3 + global / 5 + neighbor / 2;
        if (background < 4500) score -= 9000;
        if (global < 3800) score -= 5000;
        if (neighbor < 5200) score -= 7000;
        if (score > best_score) {
            best_score = score;
            best = color;
        }
    }
    return best;
}
