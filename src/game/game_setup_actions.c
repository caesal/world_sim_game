#include "game/game.h"

#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "sim/regions.h"
#include "sim/ports.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

void game_request_regenerate_regions(void) {
    if (!world_generated || civ_count > 0) return;
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    route_potential_rebuild();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    render_snapshot_publish_from_live_state();
}

int game_request_add_civilization_from_selection(const char *name, char symbol,
                                                int military, int logistics,
                                                int governance, int cohesion,
                                                int production, int commerce,
                                                int innovation) {
    int x = -1;
    int y = -1;
    int before_count = civ_count;

    if (!world_generated) return -1;
    state_write_lock();
    if (selected_x >= 0 && selected_y >= 0 &&
        is_land(world[selected_y][selected_x].geography) &&
        world[selected_y][selected_x].owner == -1) {
        x = selected_x;
        y = selected_y;
    }
    if (!add_civilization_at(name, symbol, military, logistics, governance, cohesion,
                             production, commerce, innovation, x, y)) {
        state_write_unlock();
        return -1;
    }
    selected_civ = simulation_last_created_civ_id();
    if (selected_civ < 0) selected_civ = before_count;
    event_log_push_structured(EVENT_TYPE_CIV_CREATED, EVENT_SEVERITY_INFO,
                              selected_civ, -1, -1, civs[selected_civ].capital_city, 0, 0, NULL);
    state_write_unlock();
    render_snapshot_publish_from_live_state();
    return selected_civ;
}

int game_request_edit_selected_civilization(const char *name, char symbol,
                                            int military, int logistics,
                                            int governance, int cohesion,
                                            int production, int commerce,
                                            int innovation) {
    int civ_id = selected_civ;

    if (civ_id < 0 || civ_id >= civ_count) {
        if (selected_x >= 0 && selected_y >= 0) civ_id = world[selected_y][selected_x].owner;
    }
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    state_write_lock();
    simulation_apply_civilization_edit(civ_id, name, symbol, military, logistics, governance,
                                       cohesion, production, commerce, innovation);
    selected_civ = civ_id;
    state_write_unlock();
    render_snapshot_publish_from_live_state();
    return 1;
}
