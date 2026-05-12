#include "sim/territory_integrity.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/profiler.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/sea_lanes.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/terrain_query.h"

#include <string.h>

#define ENCLAVE_LIMIT_MONTHS 300

static TerritoryIntegrityStats stats_cache[MAX_CIVS];
static unsigned char capital_connected[MAX_CIVS][MAX_NATURAL_REGIONS];
typedef struct { int own, route, sea_own, cities, regions, tech, ports; } IntegrityKey;
static IntegrityKey stats_key;

static int valid_civ(int civ_id) { return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive; }

void territory_integrity_reset(void) {
    int i;
    memset(stats_cache, 0, sizeof(stats_cache));
    memset(capital_connected, 0, sizeof(capital_connected));
    memset(&stats_key, 0, sizeof(stats_key));
    for (i = 0; i < MAX_NATURAL_REGIONS; i++) {
        natural_regions[i].disconnected_months = 0;
        natural_regions[i].disconnected_component_id = -1;
    }
}

static int city_valid_capital(int civ_id, int city_id) {
    int x;
    int y;
    int region_id;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive || cities[city_id].owner != civ_id) return 0;
    x = cities[city_id].x;
    y = cities[city_id].y;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || !is_land(world[y][x].geography)) return 0;
    region_id = world[y][x].region_id;
    return world[y][x].owner == civ_id ||
           (region_id >= 0 && region_id < region_count && natural_regions[region_id].owner_civ == civ_id);
}

static int capital_city_score(int civ_id, int city_id) {
    int region_id;
    int score;
    if (!city_valid_capital(civ_id, city_id)) return -1000000;
    region_id = regions_region_for_city(city_id);
    score = cities[city_id].population + (cities[city_id].port ? 2200 : 0);
    if (region_id >= 0) {
        const NaturalRegion *r = &natural_regions[region_id];
        score += r->tile_count * 3 + r->development_score * 4 + r->resource_diversity * 120;
    }
    return score;
}

static int choose_capital_city(int civ_id) {
    int i;
    int best = -1;
    int best_score = -1000000;
    for (i = 0; i < city_count; i++) {
        int score;
        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        score = capital_city_score(civ_id, i);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

void territory_integrity_repair_capitals(void) {
    int civ_id;
    int city_id;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        if (!civs[civ_id].alive) continue;
        if (!city_valid_capital(civ_id, civs[civ_id].capital_city)) {
            civs[civ_id].capital_city = choose_capital_city(civ_id);
        }
        for (city_id = 0; city_id < city_count; city_id++) {
            if (cities[city_id].owner == civ_id) cities[city_id].capital = city_id == civs[civ_id].capital_city;
        }
    }
}

static int region_has_port_city(int civ_id, int region_id) {
    int i;
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != civ_id || !cities[i].port) continue;
        if (regions_region_for_city(i) == region_id) return 1;
    }
    return 0;
}

static int regions_have_sea_link(int civ_id, int a, int b) {
    int i;
    int j;
    if (!region_has_port_city(civ_id, a) || !region_has_port_city(civ_id, b)) return 0;
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != civ_id || !cities[i].port || regions_region_for_city(i) != a) continue;
        for (j = 0; j < city_count; j++) {
            if (!cities[j].alive || cities[j].owner != civ_id || !cities[j].port || regions_region_for_city(j) != b) continue;
            if (sea_lanes_network_connected(i, j)) return 1;
        }
    }
    return 0;
}

static void mark_capital_connected(int civ_id, int capital_region) {
    int queue[MAX_NATURAL_REGIONS];
    int head = 0;
    int tail = 0;
    memset(capital_connected[civ_id], 0, MAX_NATURAL_REGIONS);
    if (capital_region < 0 || capital_region >= region_count) return;
    capital_connected[civ_id][capital_region] = 1;
    queue[tail++] = capital_region;
    while (head < tail) {
        NaturalRegion *r = &natural_regions[queue[head++]];
        int i;
        for (i = 0; i < r->neighbor_count; i++) {
            int n = r->neighbors[i];
            if (n < 0 || n >= region_count || capital_connected[civ_id][n]) continue;
            if (natural_regions[n].owner_civ != civ_id) continue;
            capital_connected[civ_id][n] = 1;
            queue[tail++] = n;
        }
        if (!region_has_port_city(civ_id, r->id)) continue;
        for (i = 0; i < region_count; i++) {
            if (capital_connected[civ_id][i] || natural_regions[i].owner_civ != civ_id) continue;
            if (regions_have_sea_link(civ_id, r->id, i)) {
                capital_connected[civ_id][i] = 1;
                queue[tail++] = i;
            }
        }
    }
}

static int civ_region_count(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) if (natural_regions[i].owner_civ == civ_id) count++;
    return count;
}

static int candidate_score(int candidate, int land_touch, int port_touch) {
    int score;
    if (!valid_civ(candidate)) return -1000000;
    score = (land_touch ? 1000 : 0) + (port_touch ? 500 : 0);
    score += war_current_soldiers_for_civ(candidate) * 3;
    score += civs[candidate].population / 1000;
    score += clamp(civs[candidate].tech_stage, 0, 10) * 500;
    score += civ_region_count(candidate) * 80;
    score += expansion_resource_score_for_civ(candidate) * 50;
    score -= civs[candidate].disorder * 100;
    return score;
}

static int component_has_port_contact(const int *regions, int count, int candidate) {
    int i;
    int a;
    int b;
    for (i = 0; i < count; i++) {
        for (a = 0; a < city_count; a++) {
            if (!cities[a].alive || !cities[a].port || regions_region_for_city(a) != regions[i]) continue;
            for (b = 0; b < city_count; b++) {
                if (!cities[b].alive || cities[b].owner != candidate || !cities[b].port) continue;
                if (sea_lanes_network_connected(a, b)) return 1;
            }
        }
    }
    return 0;
}

static int best_component_candidate(int owner, const int *regions, int count, int *out_score) {
    int best = -1;
    int best_score = -1000000;
    int c;
    for (c = 0; c < civ_count; c++) {
        int land = 0;
        int port = 0;
        int i;
        int score;
        if (c == owner || !valid_civ(c)) continue;
        for (i = 0; i < count && !land; i++) {
            int n;
            NaturalRegion *r = &natural_regions[regions[i]];
            for (n = 0; n < r->neighbor_count; n++) {
                int neighbor = r->neighbors[n];
                if (neighbor >= 0 && neighbor < region_count && natural_regions[neighbor].owner_civ == c) land = 1;
            }
        }
        port = component_has_port_contact(regions, count, c);
        if (!land && !port) continue;
        score = candidate_score(c, land, port);
        if (score > best_score || (score == best_score && c < best)) {
            best_score = score;
            best = c;
        }
    }
    if (out_score) *out_score = best_score;
    return best;
}

static int best_component_city(const int *regions, int count, int owner) {
    int best = -1;
    int best_pop = -1;
    int i;
    for (i = 0; i < city_count; i++) {
        int r;
        int k;
        if (!cities[i].alive || cities[i].owner != owner) continue;
        r = regions_region_for_city(i);
        for (k = 0; k < count; k++) {
            if (regions[k] == r && cities[i].population > best_pop) {
                best_pop = cities[i].population;
                best = i;
            }
        }
    }
    return best;
}

static void refresh_after_integrity_change(void) {
    territory_integrity_repair_capitals();
    world_recalculate_territory();
    population_sync_all();
    ports_refresh_city_regions();
    maritime_mark_ownership_dirty();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    dirty_mark_territory();
    dirty_mark_labels();
}

static int claim_component_to(int owner, const int *regions, int count, int preferred_city) {
    int i;
    for (i = 0; i < count; i++) {
        int city = i == 0 ? preferred_city : -1;
        if (!regions_claim_for_civ(regions[i], owner, city, 1)) return 0;
        natural_regions[regions[i]].disconnected_months = 0;
        natural_regions[regions[i]].disconnected_component_id = -1;
    }
    return 1;
}

static int create_independent_component(int owner, const int *regions, int count, int months) {
    Civilization parent = civs[owner];
    int child_id = civilization_allocate_slot(1);
    int seed_region = regions[0];
    int city_id;
    if (child_id < 0) {
        event_log_push_structured(EVENT_TYPE_ENCLAVE_FAILED, EVENT_SEVERITY_DANGER,
                                  owner, -1, seed_region, -1, months, count, "country limit reached");
        return 0;
    }
    civilization_assign_generated_name(&civs[child_id], civilization_pick_unused_name_id());
    civs[child_id].symbol = (char)('a' + (child_id % 26));
    civs[child_id].color = civilization_pick_distinct_color(child_id, 0, owner, seed_region);
    civs[child_id].alive = 1;
    civs[child_id].aggression = parent.aggression;
    civs[child_id].expansion = parent.expansion;
    civs[child_id].defense = parent.defense;
    civs[child_id].culture = parent.culture;
    civs[child_id].governance = parent.governance;
    civs[child_id].cohesion = parent.cohesion;
    civs[child_id].production = parent.production;
    civs[child_id].military = parent.military;
    civs[child_id].commerce = parent.commerce;
    civs[child_id].logistics = parent.logistics;
    civs[child_id].innovation = parent.innovation;
    civs[child_id].adaptation = parent.adaptation;
    civs[child_id].tech_stage = clamp(parent.tech_stage, 0, 10);
    civs[child_id].tech_progress = parent.tech_progress;
    civs[child_id].disorder = 35;
    civs[child_id].disorder_plague = clamp(parent.disorder_plague / 3, 0, 15);
    civs[child_id].disorder_migration = 15;
    civs[child_id].disorder_stability = 12;
    civs[child_id].collapse_grace_months = 300;
    city_id = best_component_city(regions, count, owner);
    if (city_id < 0) {
        NaturalRegion *r = &natural_regions[seed_region];
        city_id = world_create_city(child_id, r->capital_x, r->capital_y, max(900, r->average_stats.pop_capacity * 450), 1);
    }
    if (city_id >= 0) {
        cities[city_id].owner = child_id;
        cities[city_id].capital = 1;
        civs[child_id].capital_city = city_id;
    }
    if (!claim_component_to(child_id, regions, count, city_id)) {
        event_log_push_structured(EVENT_TYPE_ENCLAVE_FAILED, EVENT_SEVERITY_DANGER,
                                  owner, -1, seed_region, -1, months, count, "claim failed");
        civs[child_id].alive = 0;
        return 0;
    }
    disorder_add_war_pressure(owner, 8 + count / 2);
    event_log_push_structured(EVENT_TYPE_ENCLAVE_INDEPENDENT, EVENT_SEVERITY_WARNING,
                              child_id, owner, seed_region, city_id, months, count, NULL);
    return 1;
}

static int separate_component(int owner, const int *regions, int count, int months) {
    int score = 0;
    int target = best_component_candidate(owner, regions, count, &score);
    if (target >= 0) {
        int city = best_component_city(regions, count, owner);
        if (!claim_component_to(target, regions, count, city)) {
            event_log_push_structured(EVENT_TYPE_ENCLAVE_FAILED, EVENT_SEVERITY_DANGER,
                                      owner, target, regions[0], -1, months, count, "claim failed");
            return 0;
        }
        disorder_add_war_pressure(owner, 8 + count / 2);
        disorder_add_migration_pressure(target, 3 + count / 3);
        event_log_push_structured(EVENT_TYPE_ENCLAVE_JOINED, EVENT_SEVERITY_WARNING,
                                  owner, target, regions[0], city, months, count, NULL);
        return 1;
    }
    return create_independent_component(owner, regions, count, months);
}

static int process_disconnected_components(int civ_id, int elapsed_months, int apply) {
    static unsigned char seen[MAX_NATURAL_REGIONS];
    int component_id = 1;
    int i;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < region_count; i++) {
        int queue[MAX_NATURAL_REGIONS];
        int component[MAX_NATURAL_REGIONS];
        int head = 0;
        int tail = 0;
        int count = 0;
        int max_months = 0;
        if (seen[i] || natural_regions[i].owner_civ != civ_id || capital_connected[civ_id][i]) continue;
        seen[i] = 1;
        queue[tail++] = i;
        while (head < tail) {
            NaturalRegion *r = &natural_regions[queue[head++]];
            int n;
            component[count++] = r->id;
            r->disconnected_component_id = component_id;
            if (apply) r->disconnected_months = min(ENCLAVE_LIMIT_MONTHS, r->disconnected_months + elapsed_months);
            max_months = max(max_months, r->disconnected_months);
            for (n = 0; n < r->neighbor_count; n++) {
                int neighbor = r->neighbors[n];
                if (neighbor < 0 || neighbor >= region_count || seen[neighbor]) continue;
                if (natural_regions[neighbor].owner_civ != civ_id || capital_connected[civ_id][neighbor]) continue;
                seen[neighbor] = 1;
                queue[tail++] = neighbor;
            }
        }
        stats_cache[civ_id].disconnected_components++;
        stats_cache[civ_id].longest_disconnected_months =
            max(stats_cache[civ_id].longest_disconnected_months, max_months);
        component_id++;
        if (apply && max_months >= ENCLAVE_LIMIT_MONTHS && separate_component(civ_id, component, count, max_months)) return 1;
    }
    return 0;
}

static void rebuild_civ(int civ_id, int elapsed_months, int apply) {
    int i;
    int cap_city = civs[civ_id].capital_city;
    int cap_region = city_valid_capital(civ_id, cap_city) ? regions_region_for_city(cap_city) : -1;
    TerritoryIntegrityStats *s = &stats_cache[civ_id];
    memset(s, 0, sizeof(*s));
    s->capital_region = cap_region;
    s->best_candidate_civ = -1;
    mark_capital_connected(civ_id, cap_region);
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].owner_civ != civ_id) continue;
        s->owned_regions++;
        if (capital_connected[civ_id][i]) {
            s->capital_connected_regions++;
            if (apply) {
                natural_regions[i].disconnected_months = 0;
                natural_regions[i].disconnected_component_id = -1;
            }
        }
    }
    if (s->owned_regions > 0) s->capital_connected_percent = s->capital_connected_regions * 100 / s->owned_regions;
    if (process_disconnected_components(civ_id, elapsed_months, apply)) refresh_after_integrity_change();
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].owner_civ != civ_id || capital_connected[civ_id][i]) continue;
        if (natural_regions[i].disconnected_months == s->longest_disconnected_months) {
            int one[1] = {i};
            s->best_candidate_civ = best_component_candidate(civ_id, one, 1, &s->best_candidate_score);
            break;
        }
    }
}

static IntegrityKey make_key(void) {
    IntegrityKey k = {dirty_revision_ownership(), maritime_route_revision(), maritime_ownership_revision(),
                      city_count, region_count, 0, 0};
    int i;
    for (i = 0; i < civ_count; i++) if (civs[i].alive) k.tech = k.tech * 33 + clamp(civs[i].tech_stage, 0, 10) + 1;
    for (i = 0; i < city_count; i++) if (cities[i].alive) k.ports = k.ports * 33 + cities[i].owner * 7 + cities[i].port_region * 3 + cities[i].port;
    return k;
}

static int same_key(IntegrityKey a, IntegrityKey b) { return memcmp(&a, &b, sizeof(a)) == 0; }

static void update_port_diagnostics(int civ_id) {
    TerritoryIntegrityStats *s = &stats_cache[civ_id];
    unsigned char nets[MAX_CITIES];
    int i, j;
    memset(nets, 0, sizeof(nets));
    for (i = 0; i < city_count; i++) {
        int r = regions_region_for_city(i);
        int net;
        if (!cities[i].alive || cities[i].owner != civ_id || !cities[i].port || r < 0) continue;
        net = sea_lanes_city_network(i);
        if (capital_connected[civ_id][r]) {
            s->capital_core_port_count++;
            if (net >= 0 && net < MAX_CITIES && !nets[net]) { nets[net] = 1; s->capital_core_network_count++; }
        } else {
            s->disconnected_has_port = 1;
            if (net >= 0) s->disconnected_has_network = 1;
            for (j = 0; j < city_count && !s->disconnected_network_matches_capital; j++) {
                int cr = regions_region_for_city(j);
                if (cities[j].alive && cities[j].owner == civ_id && cities[j].port && cr >= 0 &&
                    capital_connected[civ_id][cr] && sea_lanes_network_connected(i, j)) s->disconnected_network_matches_capital = 1;
            }
        }
    }
}

static void rebuild_all(int elapsed_months, int apply) {
    int civ_id;
    memset(capital_connected, 0, sizeof(capital_connected));
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        if (valid_civ(civ_id)) rebuild_civ(civ_id, elapsed_months, apply);
        else memset(&stats_cache[civ_id], 0, sizeof(stats_cache[civ_id]));
    }
    for (civ_id = 0; civ_id < civ_count; civ_id++) if (valid_civ(civ_id)) update_port_diagnostics(civ_id);
    stats_key = make_key();
}

static void ensure_cache(void) {
    if (!same_key(stats_key, make_key())) rebuild_all(0, 0);
}

void territory_integrity_update_year(void) {
    DWORD start = GetTickCount();
    territory_integrity_repair_capitals();
    rebuild_all(12, 1);
    profiler_record_phase("Territory Integrity", (int)(GetTickCount() - start));
}

void territory_integrity_get_stats(int civ_id, TerritoryIntegrityStats *out) {
    if (!out) return;
    ensure_cache();
    memset(out, 0, sizeof(*out));
    if (civ_id >= 0 && civ_id < MAX_CIVS) *out = stats_cache[civ_id];
}

int territory_integrity_region_is_capital_connected(int civ_id, int region_id) {
    ensure_cache();
    return civ_id >= 0 && civ_id < MAX_CIVS && region_id >= 0 && region_id < MAX_NATURAL_REGIONS &&
           capital_connected[civ_id][region_id];
}

int territory_integrity_region_score(int civ_id, int region_id) {
    const NaturalRegion *r;
    int own_neighbors = 0;
    int connected_neighbors = 0;
    int i;
    if (region_id < 0 || region_id >= region_count || civ_id < 0 || civ_id >= civ_count) return 0;
    ensure_cache();
    r = &natural_regions[region_id];
    for (i = 0; i < r->neighbor_count; i++) {
        int n = r->neighbors[i];
        if (n < 0 || n >= region_count || natural_regions[n].owner_civ != civ_id) continue;
        own_neighbors++;
        if (capital_connected[civ_id][n]) connected_neighbors++;
    }
    if (stats_cache[civ_id].disconnected_components > 0 && connected_neighbors > 0) return 140 + own_neighbors * 16;
    if (connected_neighbors > 0) return 70 + own_neighbors * 12;
    if (own_neighbors > 0) return -80 + own_neighbors * 8;
    return -180;
}
