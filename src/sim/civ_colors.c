#include "sim/civ_colors.h"

#include "core/game_types.h"
#include "sim/regions.h"

#include <stdio.h>

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
static int min_background_distance(Color32 color);
static int min_background_hsl_distance(Color32 color);

typedef struct {
    int h;
    int s;
    int l;
} HslColor;

static int color_distance(Color32 a, Color32 b) {
    int dr = channel_r(a) - channel_r(b);
    int dg = channel_g(a) - channel_g(b);
    int db = channel_b(a) - channel_b(b);
    return dr * dr + dg * dg + db * db;
}

static int abs_int(int value) { return value < 0 ? -value : value; }

static HslColor color_to_hsl(Color32 color) {
    int r = channel_r(color);
    int g = channel_g(color);
    int b = channel_b(color);
    int maxc = max(r, max(g, b));
    int minc = min(r, min(g, b));
    int delta = maxc - minc;
    HslColor hsl = {0, 0, (maxc + minc) * 100 / 510};

    if (delta <= 0) return hsl;
    hsl.s = hsl.l < 50 ? delta * 100 / max(1, maxc + minc) :
            delta * 100 / max(1, 510 - maxc - minc);
    if (maxc == r) hsl.h = 60 * (g - b) / delta;
    else if (maxc == g) hsl.h = 120 + 60 * (b - r) / delta;
    else hsl.h = 240 + 60 * (r - g) / delta;
    if (hsl.h < 0) hsl.h += 360;
    return hsl;
}

static int hsl_distance(Color32 a, Color32 b) {
    HslColor ha = color_to_hsl(a);
    HslColor hb = color_to_hsl(b);
    int hue = abs_int(ha.h - hb.h);
    int sat = abs_int(ha.s - hb.s);
    int light = abs_int(ha.l - hb.l);

    if (hue > 180) hue = 360 - hue;
    return hue * 5 + sat * 3 + light * 4;
}

static int colors_too_similar(Color32 a, Color32 b) {
    HslColor ha = color_to_hsl(a);
    HslColor hb = color_to_hsl(b);
    int hue = abs_int(ha.h - hb.h);

    if (a == b) return 1;
    if (hue > 180) hue = 360 - hue;
    return hsl_distance(a, b) < 165 || (hue < 18 && abs_int(ha.l - hb.l) < 24);
}

static int color_readable_on_background(Color32 color) {
    return min_background_distance(color) >= 5600 && min_background_hsl_distance(color) >= 185;
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

static int min_background_hsl_distance(Color32 color) {
    int i;
    int best = 100000000;

    for (i = 0; i < (int)(sizeof(BACKGROUND_COLORS) / sizeof(BACKGROUND_COLORS[0])); i++) {
        int dist = hsl_distance(color, BACKGROUND_COLORS[i]);
        if (dist < best) best = dist;
    }
    return best;
}

static int neighbor_color_distance(Color32 color, int civ_id, int seed_region) {
    const NaturalRegion *region = regions_get(seed_region);
    int best = 100000000;
    int i;

    if (region) {
        for (i = 0; i < region->neighbor_count; i++) {
            int neighbor = region->neighbors[i];
            int owner;
            if (neighbor < 0 || neighbor >= region_count) continue;
            owner = natural_regions[neighbor].owner_civ;
            if (owner >= 0 && owner < civ_count && owner != civ_id && civs[owner].alive) {
                int dist = hsl_distance(color, civs[owner].color);
                if (dist < best) best = dist;
            }
        }
    }
    if (civ_id >= 0 && civ_id < civ_count) {
        int r;
        for (r = 0; r < region_count; r++) {
            const NaturalRegion *owned = &natural_regions[r];
            int n;
            if (!owned->alive || owned->owner_civ != civ_id) continue;
            for (n = 0; n < owned->neighbor_count; n++) {
                int nr = owned->neighbors[n];
                int owner;
                if (nr < 0 || nr >= region_count) continue;
                owner = natural_regions[nr].owner_civ;
                if (owner >= 0 && owner < civ_count && owner != civ_id && civs[owner].alive) {
                    int dist = hsl_distance(color, civs[owner].color);
                    if (dist < best) best = dist;
                }
            }
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
        dist = hsl_distance(color, civs[i].color);
        if (dist < best) best = dist;
    }
    return best;
}

static int color_candidate_score(Color32 color, int civ_id, int parent_civ_id, int seed_region) {
    int background_rgb = min_background_distance(color);
    int background_hsl = min_background_hsl_distance(color);
    int global = global_color_distance(color, civ_id);
    int neighbor = neighbor_color_distance(color, civ_id, seed_region);
    int parent = 100000000;
    int i;
    int score = background_rgb / 18 + background_hsl * 6 + global * 7 + neighbor * 14;

    if (parent_civ_id >= 0 && parent_civ_id < civ_count) {
        parent = hsl_distance(color, civs[parent_civ_id].color);
        score += parent * 12;
    }
    for (i = 0; i < civ_count; i++) {
        if (i == civ_id || !civs[i].alive) continue;
        if (color == civs[i].color) score -= 200000;
        if (colors_too_similar(color, civs[i].color)) score -= 20000;
    }
    if (parent < 190) score -= 28000;
    if (neighbor < 210) score -= 45000;
    if (global < 165) score -= 18000;
    if (background_rgb < 4500 || background_hsl < 160) score -= 24000;
    return score;
}

static int preferred_color_is_safe(Color32 color, int civ_id, int parent_civ_id, int seed_region) {
    int i;

    if (!color_readable_on_background(color)) return 0;
    if (parent_civ_id >= 0 && parent_civ_id < civ_count &&
        colors_too_similar(color, civs[parent_civ_id].color)) return 0;
    if (neighbor_color_distance(color, civ_id, seed_region) < 210) return 0;
    for (i = 0; i < civ_count; i++) {
        if (i == civ_id || !civs[i].alive) continue;
        if (color == civs[i].color || colors_too_similar(color, civs[i].color)) return 0;
    }
    return 1;
}

static Color32 fallback_color(int civ_id, int attempt) {
    int hue = (civ_id * 137 + attempt * 47) % 360;
    int segment = hue / 60;
    int rem = hue % 60;
    int c = 168;
    int x = c * (60 - abs_int(rem * 2 - 60)) / 60;
    int m = 48 + (attempt % 3) * 14;
    int r = m;
    int g = m;
    int b = m;

    if (segment == 0) { r += c; g += x; }
    else if (segment == 1) { r += x; g += c; }
    else if (segment == 2) { g += c; b += x; }
    else if (segment == 3) { g += x; b += c; }
    else if (segment == 4) { r += x; b += c; }
    else { r += c; b += x; }
    return COLOR32_RGB(clamp(r, 42, 238), clamp(g, 42, 238), clamp(b, 42, 238));
}

Color32 civilization_pick_distinct_color(int civ_id, Color32 preferred_color,
                                         int parent_civ_id, int seed_region) {
    int i;
    int best_score = -100000000;
    Color32 best = POLITICAL_PALETTE[0];
    int palette_count = (int)(sizeof(POLITICAL_PALETTE) / sizeof(POLITICAL_PALETTE[0]));
    int has_preferred = preferred_color != 0;

    if (has_preferred) {
        if (preferred_color_is_safe(preferred_color, civ_id, parent_civ_id, seed_region)) return preferred_color;
    }
    for (i = 0; i < palette_count; i++) {
        int index = (i + civ_id * 7) % palette_count;
        Color32 color = POLITICAL_PALETTE[index];
        int score = color_candidate_score(color, civ_id, parent_civ_id, seed_region);
        if (score > best_score) {
            best_score = score;
            best = color;
        }
    }
    for (i = 0; i < MAX_CIVS; i++) {
        Color32 color = CIV_COLORS[i];
        int score = color_candidate_score(color, civ_id, parent_civ_id, seed_region);
        if (score > best_score) {
            best_score = score;
            best = color;
        }
    }
    for (i = 0; i < 48; i++) {
        Color32 color = fallback_color(civ_id, i);
        int score = color_candidate_score(color, civ_id, parent_civ_id, seed_region);
        if (score > best_score) {
            best_score = score;
            best = color;
        }
    }
    if (has_preferred && best != preferred_color) {
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_INFO,
                                  civ_id, -1, -1, -1, 0, 0,
                                  "[Debug] Adjusted requested country color to avoid a duplicate or low-contrast neighbor.");
    } else if (best_score < 0) {
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_INFO,
                                  civ_id, -1, -1, -1, 0, 0,
                                  "[Debug] Country color picker used a low-confidence fallback color.");
    }
    return best;
}

Color32 civilization_pick_auto_color(int civ_id, int seed_region) {
    return civilization_pick_distinct_color(civ_id, 0, -1, seed_region);
}

int civilization_colors_debug_check(void) {
    int issues = 0;
    int i;
    int r;

    for (i = 0; i < civ_count; i++) {
        int j;
        if (!civs[i].alive) continue;
        for (j = i + 1; j < civ_count; j++) {
            if (!civs[j].alive) continue;
            if (civs[i].color == civs[j].color || colors_too_similar(civs[i].color, civs[j].color)) issues++;
        }
    }
    for (r = 0; r < region_count; r++) {
        NaturalRegion *region = &natural_regions[r];
        int n;
        int owner = region->owner_civ;
        if (!region->alive || owner < 0 || owner >= civ_count || !civs[owner].alive) continue;
        for (n = 0; n < region->neighbor_count; n++) {
            int neighbor = region->neighbors[n];
            int other;
            if (neighbor < 0 || neighbor >= region_count) continue;
            other = natural_regions[neighbor].owner_civ;
            if (other <= owner || other >= civ_count || !civs[other].alive) continue;
            if (colors_too_similar(civs[owner].color, civs[other].color)) issues++;
        }
    }
    if (issues > 0) {
        char text[EVENT_LOG_LEN];
        snprintf(text, sizeof(text), "[Debug] Country color check found %d duplicate/near-duplicate issue%s.",
                 issues, issues == 1 ? "" : "s");
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, -1, -1, issues, 0, text);
    }
    return issues;
}

int civilization_repair_alive_colors(void) {
    int changed = 0;
    int i;

    for (i = 0; i < civ_count; i++) {
        int seed_region = -1;
        Color32 old_color;
        if (!civs[i].alive) continue;
        if (civs[i].capital_city >= 0 && civs[i].capital_city < city_count) {
            seed_region = regions_region_for_city(civs[i].capital_city);
        }
        if (preferred_color_is_safe(civs[i].color, i, -1, seed_region)) continue;
        old_color = civs[i].color;
        civs[i].color = civilization_pick_distinct_color(i, 0, -1, seed_region);
        if (civs[i].color != old_color) changed++;
    }
    if (changed > 0) {
        char text[EVENT_LOG_LEN];
        snprintf(text, sizeof(text), "[Debug] Repaired %d country color%s for readability.",
                 changed, changed == 1 ? "" : "s");
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_INFO,
                                  -1, -1, -1, -1, changed, 0, text);
    }
    return changed;
}
