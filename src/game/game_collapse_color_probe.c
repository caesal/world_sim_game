#include "game/game_collapse_color_probe.h"

#include "game/game.h"
#include "core/game_types.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/economy.h"
#include "sim/enclave_resolution.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLLAPSE_COLOR_DIR "build/validation/collapse_color_probe_20260628"
#define BORDER_DISTANCE_MIN 210
#define PARENT_DISTANCE_MIN 190

static void ensure_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(COLLAPSE_COLOR_DIR, NULL);
}

static void reset_fixture(void) {
    int i;
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    memset(world, 0, sizeof(world));
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    for (i = 0; i < MAX_CIVS; i++) civilization_reset_slot_state(i);
    civ_count = 0;
    region_count = 0;
    city_count = 0;
    world_generated = 1;
    selected_civ = selected_x = selected_y = -1;
    auto_run = 0;
    event_log_clear();
}

static void add_neighbor(int a, int b) {
    NaturalRegion *ra = &natural_regions[a];
    NaturalRegion *rb = &natural_regions[b];
    if (ra->neighbor_count < MAX_REGION_NEIGHBORS) ra->neighbors[ra->neighbor_count++] = b;
    if (rb->neighbor_count < MAX_REGION_NEIGHBORS) rb->neighbors[rb->neighbor_count++] = a;
}

static void setup_civ(int id, const char *name, Color32 color, int capital_region) {
    civilization_reset_slot_state(id);
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].heritage = CIV_HERITAGE_WESTERN;
    civs[id].color = color;
    civs[id].capital_city = capital_region;
    civs[id].governance = civs[id].cohesion = civs[id].commerce = 8;
    civs[id].military = civs[id].production = civs[id].logistics = 6;
    civs[id].culture = civs[id].innovation = civs[id].adaptation = 6;
    economy_initialize_civ(id);
}

static void setup_line_fixture(int parent_regions) {
    int i;
    reset_fixture();
    civ_count = 2;
    region_count = parent_regions + 1;
    city_count = region_count;
    setup_civ(0, "Parent", COLOR32_RGB(72, 112, 224), 0);
    setup_civ(1, "Border", COLOR32_RGB(194, 74, 58), parent_regions);
    for (i = 0; i < region_count; i++) {
        int owner = i < parent_regions ? 0 : 1;
        natural_regions[i].id = i;
        natural_regions[i].alive = 1;
        natural_regions[i].tile_count = 1;
        natural_regions[i].owner_civ = owner;
        natural_regions[i].city_id = i;
        natural_regions[i].center_x = natural_regions[i].capital_x = i;
        natural_regions[i].center_y = natural_regions[i].capital_y = 0;
        natural_regions[i].average_stats.pop_capacity = 6;
        natural_regions[i].average_stats.habitability = 6;
        natural_regions[i].average_stats.food = 6;
        natural_regions[i].total_stats.food = 12;
        cities[i].alive = 1;
        cities[i].owner = owner;
        cities[i].x = i;
        cities[i].y = 0;
        cities[i].capital = i == civs[owner].capital_city;
        population_init_city(i, owner == 0 ? 22000 : 18000);
        world[0][i].geography = GEO_PLAIN;
        world[0][i].owner = owner;
        world[0][i].region_id = i;
        world[0][i].province_id = i;
    }
    for (i = 0; i + 1 < region_count; i++) add_neighbor(i, i + 1);
    world_invalidate_region_cache();
    population_sync_all();
    economy_initialize_civ(0);
    economy_initialize_civ(1);
    civs[0].treasury = 1000;
    civs[1].treasury = 300;
    economy_normalize_civ(0);
    economy_normalize_civ(1);
}

static int min_border_distance(int civ_id) {
    int best = 100000000;
    int r;
    for (r = 0; r < region_count; r++) {
        int n;
        if (!natural_regions[r].alive || natural_regions[r].owner_civ != civ_id) continue;
        for (n = 0; n < natural_regions[r].neighbor_count; n++) {
            int nr = natural_regions[r].neighbors[n];
            int owner;
            int dist;
            if (nr < 0 || nr >= region_count) continue;
            owner = natural_regions[nr].owner_civ;
            if (owner < 0 || owner == civ_id || owner >= civ_count || !civs[owner].alive) continue;
            dist = civilization_color_display_distance(civs[civ_id].color, civs[owner].color);
            if (dist < best) best = dist;
        }
    }
    return best;
}

static int write_bmp(const char *path) {
    const int cell = 8;
    const int width = region_count * cell;
    const int height = 24;
    const int row_bytes = ((width * 3 + 3) / 4) * 4;
    const int image_bytes = row_bytes * height;
    unsigned char header[54] = {0};
    FILE *file = fopen(path, "wb");
    int x, y;
    if (!file) return 0;
    header[0] = 'B'; header[1] = 'M';
    *(int *)&header[2] = 54 + image_bytes;
    *(int *)&header[10] = 54;
    *(int *)&header[14] = 40;
    *(int *)&header[18] = width;
    *(int *)&header[22] = height;
    header[26] = 1; header[28] = 24;
    *(int *)&header[34] = image_bytes;
    fwrite(header, 1, sizeof(header), file);
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            int region = x / cell;
            int owner = region >= 0 && region < region_count ? natural_regions[region].owner_civ : -1;
            Color32 color = owner >= 0 && owner < civ_count && civs[owner].alive ? civs[owner].color : 0;
            fputc((color >> 16) & 255, file);
            fputc((color >> 8) & 255, file);
            fputc(color & 255, file);
        }
        for (x = width * 3; x < row_bytes; x++) fputc(0, file);
    }
    fclose(file);
    return 1;
}

static int check_children(FILE *summary, int first_child, int parent_id, const char *label) {
    int child_count = 0;
    int min_parent = 100000000;
    int min_border = 100000000;
    int min_sibling = 100000000;
    int ok = 1;
    int i, j;
    for (i = first_child; i < civ_count; i++) {
        int parent_dist;
        int border_dist;
        if (!civs[i].alive) continue;
        child_count++;
        parent_dist = civilization_color_display_distance(civs[i].color, civs[parent_id].color);
        border_dist = min_border_distance(i);
        if (parent_dist < min_parent) min_parent = parent_dist;
        if (border_dist < min_border) min_border = border_dist;
        if (parent_dist < PARENT_DISTANCE_MIN || border_dist < BORDER_DISTANCE_MIN) ok = 0;
        fprintf(summary, "%s_child id=%d parent_dist=%d border_dist=%d color=%06X\n",
                label, i, parent_dist, border_dist, (unsigned)civs[i].color & 0xFFFFFFu);
    }
    for (i = first_child; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        for (j = i + 1; j < civ_count; j++) {
            int dist;
            if (!civs[j].alive) continue;
            dist = civilization_color_display_distance(civs[i].color, civs[j].color);
            if (dist < min_sibling) min_sibling = dist;
            if (civilization_colors_too_similar_for_display(civs[i].color, civs[j].color)) ok = 0;
        }
    }
    fprintf(summary, "%s_summary children=%d min_parent=%d min_border=%d min_sibling=%d ok=%d\n",
            label, child_count, min_parent, min_border, min_sibling, ok);
    return ok && child_count > 0;
}

static int run_player_civil_unrest_case(FILE *summary) {
    int requested;
    int ok;
    setup_line_fixture(38);
    requested = game_request_trigger_civil_unrest(0);
    ok = check_children(summary, 2, 0, "player_civil_unrest");
    write_bmp(COLLAPSE_COLOR_DIR "/player_civil_unrest_regions.bmp");
    fprintf(summary, "player_civil_unrest requested=%d alive_civs=%d artifact=%s\n",
            requested, civ_count, COLLAPSE_COLOR_DIR "/player_civil_unrest_regions.bmp");
    return requested && ok;
}

static int run_enclave_case(FILE *summary) {
    const int component[2] = {2, 3};
    int seed;
    for (seed = 0; seed < 100; seed++) {
        setup_line_fixture(4);
        srand((unsigned)seed);
        if (!enclave_resolve_component(0, component, 2, 12)) continue;
        if (civ_count > 2 && civs[2].alive && natural_regions[2].owner_civ == 2) {
            int border_dist = min_border_distance(2);
            int parent_dist = civilization_color_display_distance(civs[2].color, civs[0].color);
            int ok = border_dist >= BORDER_DISTANCE_MIN && parent_dist >= PARENT_DISTANCE_MIN;
            fprintf(summary, "enclave_child seed=%d child=2 parent_dist=%d border_dist=%d ok=%d\n",
                    seed, parent_dist, border_dist, ok);
            return ok;
        }
    }
    fprintf(summary, "enclave_child seed=-1 child=-1 ok=0\n");
    return 0;
}

int run_collapse_color_probe(void) {
    FILE *summary;
    int ok = 1;
    ensure_dirs();
    summary = fopen(COLLAPSE_COLOR_DIR "/summary.txt", "w");
    if (!summary) return 1;
    ok &= run_player_civil_unrest_case(summary);
    ok &= run_enclave_case(summary);
    fclose(summary);
    return ok ? 0 : 1;
}
