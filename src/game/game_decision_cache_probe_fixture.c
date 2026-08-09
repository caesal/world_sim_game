#include "game/game_decision_cache_probe_fixture.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "game/game_worldgen.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/diplomacy.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int last_worldgen_first_publication_ok;

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_text(uint64_t hash, const char *text) {
    return text ? hash_bytes(hash, text, strlen(text) + 1) : hash_bytes(hash, "", 1);
}

static void fill_probe_civ(Civilization *civ, int id, int uid) {
    memset(civ, 0, sizeof(*civ));
    snprintf(civ->name, sizeof(civ->name), "Decision Probe %d", id);
    civ->name_id = id;
    civ->uid = uid;
    civ->symbol = (char)('A' + id % 26);
    civ->alive = 1;
    civ->population = 10000 + id;
    civ->aggression = 4 + id % 3;
    civ->expansion = 5 + id % 4;
    civ->defense = 6;
    civ->culture = 5;
    civ->governance = 8;
    civ->cohesion = 8;
    civ->production = 6;
    civ->military = 6;
    civ->commerce = 6;
    civ->logistics = 6;
    civ->innovation = 6;
    civ->adaptation = 5;
    civ->tech_stage = 2;
    civ->capital_city = -1;
    civ->treasury = 500;
    civ->treasury_cap = 1000;
    civ->resource_pressure = id % 20;
}

void decision_cache_probe_setup_fixture(int alive_count, int uid_base) {
    int i;
    set_active_map_size(MAP_SIZE_SMALL);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities));
    memset(natural_regions, 0, sizeof(natural_regions));
    civ_count = clamp(alive_count, 0, MAX_CIVS);
    city_count = 0;
    region_count = 0;
    year = 10;
    month = 1;
    selected_x = selected_y = selected_civ = -1;
    world_generated = 1;
    for (i = 0; i < civ_count; i++) {
        fill_probe_civ(&civs[i], i, uid_base + i);
    }
    dirty_mark_world();
    decision_snapshot_cache_reset();
}

int decision_cache_probe_setup_generated_fixture(int alive_count, int uid_base) {
    DecisionSnapshotCacheDiagnostics first;
    unsigned int seed = 4400341u;
    int generated_count;
    int i;
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    ocean_slider = 45; continent_slider = 60; relief_slider = 56;
    moisture_slider = 48; drought_slider = 50; vegetation_slider = 54;
    bias_forest_slider = 58; bias_desert_slider = 38;
    bias_mountain_slider = 60; bias_wetland_slider = 42;
    game_worldgen_validation_set_next_seed(&seed);
    game_request_new_world_with_progress(NULL);
    decision_snapshot_cache_get_diagnostics(&first);
    generated_count = civ_count;
    last_worldgen_first_publication_ok = world_generated && generated_count == 26 &&
        first.published_expected_count == generated_count &&
        first.published_built_count == generated_count &&
        first.published_count == generated_count && first.published_revision > 0 &&
        first.published_year == year && first.published_month == month;
    render_snapshot_shutdown();
    alive_count = clamp(alive_count, generated_count, MAX_CIVS);
    for (i = generated_count; i < alive_count; i++) {
        fill_probe_civ(&civs[i], i, uid_base + i);
    }
    civ_count = alive_count;
    year = 10;
    month = 1;
    selected_x = selected_y = selected_civ = -1;
    dirty_mark_world();
    decision_snapshot_cache_reset();
    return last_worldgen_first_publication_ok && civ_count == alive_count &&
           region_count > 600;
}

int decision_cache_probe_worldgen_first_publication_ok(void) {
    return last_worldgen_first_publication_ok;
}

void decision_cache_probe_advance_month(void) {
    month++;
    if (month > 12) {
        month = 1;
        year++;
    }
}

int decision_cache_probe_alive_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) if (civs[i].alive) count++;
    return count;
}

uint64_t decision_cache_probe_published_hash(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        DecisionSnapshot snapshot;
        const char *main_intent;
        const char *expansion_reason;
        const char *war_reason;
        if (!civs[i].alive || !decision_snapshot_cached(i, &snapshot)) continue;
        main_intent = snapshot.main_intent;
        expansion_reason = snapshot.expansion_reason;
        war_reason = snapshot.war_reason;
        snapshot.main_intent = NULL;
        snapshot.expansion_reason = NULL;
        snapshot.war_reason = NULL;
        hash = hash_bytes(hash, &i, sizeof(i));
        hash = hash_bytes(hash, &civs[i].uid, sizeof(civs[i].uid));
        hash = hash_bytes(hash, &snapshot, sizeof(snapshot));
        hash = hash_text(hash, main_intent);
        hash = hash_text(hash, expansion_reason);
        hash = hash_text(hash, war_reason);
    }
    return hash;
}

int decision_cache_probe_verify_publication(int expected_alive, uint64_t expected_revision,
                                            int *missing, int *uid_mismatch,
                                            int *fallback_zero) {
    DecisionSnapshotCacheDiagnostics diagnostics;
    int missing_count = 0;
    int mismatch_count = 0;
    int zero_count = 0;
    int i;
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        DecisionSnapshot snapshot;
        if (!civs[i].alive) continue;
        if (!decision_snapshot_cached(i, &snapshot)) {
            missing_count++;
            continue;
        }
        if (snapshot.published_revision != expected_revision) mismatch_count++;
        if (!snapshot.main_intent || !snapshot.main_intent[0] ||
            snapshot.published_revision == 0) zero_count++;
    }
    if (missing) *missing = missing_count;
    if (uid_mismatch) *uid_mismatch = mismatch_count;
    if (fallback_zero) *fallback_zero = zero_count;
    return decision_cache_probe_alive_count() == expected_alive &&
           diagnostics.published_expected_count == expected_alive &&
           diagnostics.published_built_count == expected_alive &&
           diagnostics.published_count == expected_alive &&
           diagnostics.published_revision == expected_revision &&
           missing_count == 0 && mismatch_count == 0 && zero_count == 0;
}
