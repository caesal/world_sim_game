#include "game/game.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "sim/civ_colors.h"
#include "sim/regions.h"

void game_request_pause(void) {
    auto_run = 0;
}

void game_pause_for_modal_or_action(void) {
    game_request_pause();
}

static int color_seed_region_for_civ(int civ_id) {
    int seed_region = -1;
    if (civ_id >= 0 && civ_id < civ_count &&
        civs[civ_id].capital_city >= 0 && civs[civ_id].capital_city < city_count) {
        seed_region = regions_region_for_city(civs[civ_id].capital_city);
    } else if (selected_x >= 0 && selected_y >= 0) {
        int region_id = world[selected_y][selected_x].region_id;
        if (region_id >= 0 && region_id < region_count) seed_region = region_id;
    }
    return seed_region;
}

static void mark_color_visuals_dirty(void) {
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
}

void game_request_set_civilization_color_exact(int civ_id, Color32 color) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    state_write_lock();
    civs[civ_id].color = color;
    mark_color_visuals_dirty();
    state_write_unlock();
    render_snapshot_publish_from_live_state();
}

void game_request_set_civilization_color_auto_avoid(int civ_id, Color32 preferred_color) {
    int seed_region;
    if (civ_id < 0 || civ_id >= civ_count) return;
    state_write_lock();
    seed_region = color_seed_region_for_civ(civ_id);
    civs[civ_id].color = civilization_pick_distinct_color(civ_id, preferred_color, -1, seed_region);
    mark_color_visuals_dirty();
    state_write_unlock();
    render_snapshot_publish_from_live_state();
}

Color32 game_preview_civilization_color_auto_avoid(int civ_id, Color32 preferred_color) {
    int effective_id = civ_id >= 0 ? civ_id : civ_count;
    int seed_region = color_seed_region_for_civ(civ_id);
    return civilization_preview_distinct_color(effective_id, preferred_color, -1, seed_region);
}

void game_request_set_civilization_color(int civ_id, Color32 color) {
    game_request_set_civilization_color_exact(civ_id, color);
}
