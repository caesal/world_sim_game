#include "core/render_snapshot_plague_impact.h"

#include "core/game_types.h"
#include "sim/plague_state.h"
#include "sim/simulation.h"

#include <stdio.h>
#include <string.h>

static int valid_civ_id(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS;
}

static int valid_city_id(int city_id) {
    return city_id >= 0 && city_id < city_count && city_id < MAX_CITIES;
}

static int country_bit_is_set(const PlagueEpisodeState *episode, int civ_id) {
    if (!episode || civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    return (episode->ever_country_bits[civ_id / 64] &
            (UINT64_C(1) << (civ_id % 64))) != 0;
}

static void copy_country_identity(SnapshotPlagueCountryImpact *out, int civ_id) {
    const Civilization *civ;
    if (!out || !valid_civ_id(civ_id)) return;
    civ = &civs[civ_id];
    out->civ_id = civ_id;
    out->civ_uid = civ->uid;
    out->alive = civ->alive;
    out->symbol = civ->symbol;
    out->color = civ->color;
    out->current_population = max(0, civ->population);
    snprintf(out->name_en, sizeof(out->name_en), "%s",
             civilization_display_name_for_language(civ_id, 0));
    snprintf(out->name_zh, sizeof(out->name_zh), "%s",
             civilization_display_name_for_language(civ_id, 1));
}

static void copy_city_owner_identity(SnapshotPlagueCityImpact *out, int owner) {
    const Civilization *civ;
    out->current_owner_id = -1;
    if (!valid_civ_id(owner)) return;
    civ = &civs[owner];
    out->current_owner_id = owner;
    out->current_owner_uid = civ->uid;
    out->current_owner_alive = civ->alive;
    out->current_owner_symbol = civ->symbol;
    out->current_owner_color = civ->color;
    snprintf(out->current_owner_name_en, sizeof(out->current_owner_name_en), "%s",
             civilization_display_name_for_language(owner, 0));
    snprintf(out->current_owner_name_zh, sizeof(out->current_owner_name_zh), "%s",
             civilization_display_name_for_language(owner, 1));
}

static int city_ranks_before(const SnapshotPlagueCityImpact *a,
                             const SnapshotPlagueCityImpact *b) {
    if (a->episode_deaths != b->episode_deaths) {
        return a->episode_deaths > b->episode_deaths;
    }
    return a->city_id < b->city_id;
}

static void insert_top_city(SnapshotPlagueImpact *out,
                            const SnapshotPlagueCityImpact *city) {
    int count = out->city_count;
    int insert = count;
    int i;
    for (i = 0; i < count; i++) {
        if (city_ranks_before(city, &out->cities[i])) {
            insert = i;
            break;
        }
    }
    if (insert >= RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT) return;
    if (count < RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT) count++;
    for (i = count - 1; i > insert; i--) {
        out->cities[i] = out->cities[i - 1];
    }
    out->cities[insert] = *city;
    out->city_count = count;
}

static int death_share_basis_points(int64_t deaths, int64_t total_deaths) {
    long double scaled;
    if (deaths <= 0 || total_deaths <= 0) return 0;
    scaled = (long double)deaths * 10000.0L / (long double)total_deaths;
    return clamp((int)scaled, 0, 10000);
}

static int country_ranks_before(const SnapshotPlagueCountryImpact *a,
                                const SnapshotPlagueCountryImpact *b) {
    if (a->episode_deaths != b->episode_deaths) {
        return a->episode_deaths > b->episode_deaths;
    }
    return a->civ_id < b->civ_id;
}

static void sort_countries(SnapshotPlagueImpact *out) {
    int i;
    for (i = 1; i < out->country_count; i++) {
        SnapshotPlagueCountryImpact value = out->countries[i];
        int insert = i;
        while (insert > 0) {
            out->country_sort_comparison_count++;
            if (!country_ranks_before(&value, &out->countries[insert - 1])) break;
            out->countries[insert] = out->countries[insert - 1];
            insert--;
        }
        out->countries[insert] = value;
    }
}

static int add_country(SnapshotPlagueImpact *out, int civ_id,
                       int country_index[MAX_CIVS]) {
    SnapshotPlagueCountryImpact *country;
    if (!valid_civ_id(civ_id)) return -1;
    if (country_index[civ_id] >= 0) return country_index[civ_id];
    if (out->country_count >= MAX_CIVS) return -1;
    country_index[civ_id] = out->country_count++;
    country = &out->countries[country_index[civ_id]];
    memset(country, 0, sizeof(*country));
    copy_country_identity(country, civ_id);
    return country_index[civ_id];
}

static SnapshotPlagueCityImpact build_city(const PlagueModelState *model,
                                            int city_id, int absolute_month) {
    SnapshotPlagueCityImpact out;
    const PlagueCityEpisodeState *plague_city = &model->cities[city_id];
    memset(&out, 0, sizeof(out));
    out.city_id = city_id;
    out.current_owner_id = -1;
    out.episode_deaths = plague_city->episode_deaths;
    out.generation = plague_city->generation;
    if (!valid_city_id(city_id) || !cities[city_id].alive ||
        cities[city_id].population <= 0) {
        out.status = SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS;
        if (valid_city_id(city_id)) {
            snprintf(out.city_name, sizeof(out.city_name), "%s", cities[city_id].name);
            copy_city_owner_identity(&out, cities[city_id].owner);
        }
        return out;
    }
    snprintf(out.city_name, sizeof(out.city_name), "%s", cities[city_id].name);
    out.current_population = max(0, cities[city_id].population);
    copy_city_owner_identity(&out, cities[city_id].owner);
    if (plague_city->active) {
        out.status = SNAPSHOT_PLAGUE_IMPACT_ACTIVE;
        out.months_remaining = max(0, plague_city->recovery_month - absolute_month);
    } else {
        out.status = SNAPSHOT_PLAGUE_IMPACT_RECOVERED;
    }
    return out;
}

void render_snapshot_plague_impact_build(SnapshotPlagueImpact *out,
                                         int absolute_month) {
    const PlagueModelState *model;
    int country_index[MAX_CIVS];
    int i;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    model = plague_state_get();
    if (!model->episode.active) return;
    out->active = 1;
    out->episode_id = model->episode.episode_id;
    out->total_episode_deaths = model->episode.total_deaths;
    for (i = 0; i < MAX_CIVS; i++) country_index[i] = -1;
    for (i = 0; i < MAX_CIVS; i++) {
        if (country_bit_is_set(&model->episode, i)) add_country(out, i, country_index);
    }
    for (i = 0; i < model->ever_infected_city_count && i < MAX_CITIES; i++) {
        SnapshotPlagueCityImpact city;
        SnapshotPlagueCountryImpact *country;
        int city_id = model->ever_infected_city_ids[i];
        int owner;
        int index;
        if (city_id < 0 || city_id >= MAX_CITIES ||
            !model->cities[city_id].ever_infected_current_episode) continue;
        city = build_city(model, city_id, absolute_month);
        out->candidate_city_count++;
        insert_top_city(out, &city);
        owner = valid_city_id(city_id) ? cities[city_id].owner : -1;
        index = add_country(out, owner, country_index);
        if (index < 0) {
            out->unattributed_city_count++;
            out->unattributed_deaths += model->cities[city_id].episode_deaths;
            continue;
        }
        country = &out->countries[index];
        country->ever_infected_city_count++;
        country->episode_deaths += model->cities[city_id].episode_deaths;
        if (city.status == SNAPSHOT_PLAGUE_IMPACT_ACTIVE) {
            country->current_infected_city_count++;
        }
    }
    for (i = 0; i < out->country_count; i++) {
        SnapshotPlagueCountryImpact *country = &out->countries[i];
        country->peak_severity = model->episode.severity;
        country->death_share_basis_points = death_share_basis_points(
            country->episode_deaths, out->total_episode_deaths);
        country->status = country->current_infected_city_count > 0 ?
                          SNAPSHOT_PLAGUE_IMPACT_ACTIVE :
                          SNAPSHOT_PLAGUE_IMPACT_RECOVERED;
    }
    sort_countries(out);
}
