#include "game/game_presentation_plague_probe.h"

#include "core/game_types.h"
#include "core/render_snapshot_plague_impact.h"
#include "sim/plague_state.h"

#include <stdlib.h>
#include <string.h>

static void set_country_bit(uint64_t bits[PLAGUE_CIV_WORD_COUNT], int civ_id) {
    bits[civ_id / 64] |= UINT64_C(1) << (civ_id % 64);
}

static int fixture_setup(void) {
    static const int owners[8] = {7, 1, 2, 3, 4, 5, 6, 7};
    static const int deaths[8] = {900, 950, 500, 850, 800, 800, 600, 100};
    PlagueModelState *model;
    int i;
    civ_count = 8;
    city_count = 8;
    memset(civs, 0, sizeof(Civilization) * MAX_CIVS);
    memset(cities, 0, sizeof(City) * MAX_CITIES);
    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        civ->alive = 1;
        civ->uid = 4100 + i;
        civ->symbol = (char)('A' + i);
        civ->color = RGB(60 + i * 12, 100 + i * 9, 140 + i * 7);
        civ->custom_name = 1;
        civ->population = 500000 - i * 17000;
        snprintf(civ->name, sizeof(civ->name), "Impact Realm %d", i);
    }
    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        city->alive = i != 3;
        city->owner = owners[i];
        city->population = city->alive ? 70000 - i * 4000 : 0;
        snprintf(city->name, sizeof(city->name), "Impact City %d", i);
    }
    plague_state_reset();
    model = plague_state_mutable();
    model->episode.active = 1;
    model->episode.episode_id = 811;
    model->episode.severity = 9;
    model->episode.total_deaths = 5500;
    for (i = 0; i < 7; i++) set_country_bit(model->episode.ever_country_bits, i);
    model->ever_infected_city_count = city_count;
    for (i = 0; i < city_count; i++) {
        model->ever_infected_city_ids[i] = i;
        model->ever_infected_flags[i] = 1;
        model->cities[i].ever_infected_current_episode = 1;
        model->cities[i].active = i != 1;
        model->cities[i].generation = i % 4;
        model->cities[i].recovery_month = 1230 + i;
        model->cities[i].episode_deaths = deaths[i];
    }
    return 1;
}

static int sorted_country_case(const SnapshotPlagueImpact *impact) {
    static const int expected_ids[8] = {7, 1, 3, 4, 5, 6, 2, 0};
    static const int64_t expected_deaths[8] = {
        1000, 950, 850, 800, 800, 600, 500, 0
    };
    int i;
    if (!impact->active || impact->episode_id != 811 ||
        impact->country_count != 8 || impact->candidate_city_count != 8 ||
        impact->total_episode_deaths != 5500) return 0;
    for (i = 0; i < 8; i++) {
        if (impact->countries[i].civ_id != expected_ids[i] ||
            impact->countries[i].episode_deaths != expected_deaths[i]) return 0;
    }
    return impact->countries[3].civ_id == 4 &&
           impact->countries[4].civ_id == 5 &&
           impact->country_sort_comparison_count > 0;
}

static int top_city_case(const SnapshotPlagueImpact *impact) {
    static const int expected_ids[RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT] = {
        1, 0, 3, 4, 5
    };
    int i;
    if (impact->city_count != RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT) return 0;
    for (i = 0; i < RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT; i++) {
        if (impact->cities[i].city_id != expected_ids[i]) return 0;
    }
    return impact->cities[0].status == SNAPSHOT_PLAGUE_IMPACT_RECOVERED &&
           impact->cities[1].status == SNAPSHOT_PLAGUE_IMPACT_ACTIVE &&
           impact->cities[1].current_owner_id == 7 &&
           impact->cities[1].current_owner_uid == 4107 &&
           impact->cities[2].status ==
               SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS;
}

static int fewer_city_case(SnapshotPlagueImpact *impact) {
    PlagueModelState *model = plague_state_mutable();
    model->ever_infected_city_count = 3;
    model->ever_infected_city_ids[0] = 0;
    model->ever_infected_city_ids[1] = 1;
    model->ever_infected_city_ids[2] = 3;
    render_snapshot_plague_impact_build(impact, 1200);
    return impact->city_count == 3 && impact->candidate_city_count == 3 &&
           impact->cities[0].city_id == 1 &&
           impact->cities[1].city_id == 0 &&
           impact->cities[2].city_id == 3;
}

static int unattributed_case(SnapshotPlagueImpact *impact) {
    int saved_owner = cities[2].owner;
    cities[2].owner = -1;
    render_snapshot_plague_impact_build(impact, 1200);
    cities[2].owner = saved_owner;
    return impact->unattributed_city_count == 1 &&
           impact->unattributed_deaths == 500;
}

static int inactive_clear_case(SnapshotPlagueImpact *impact,
                               SnapshotPlagueImpact *zero) {
    PlagueModelState *model = plague_state_mutable();
    memset(impact, 0xA5, sizeof(*impact));
    model->episode.active = 0;
    render_snapshot_plague_impact_build(impact, 1200);
    return memcmp(impact, zero, sizeof(*impact)) == 0;
}

int game_presentation_plague_impact_probe(FILE *summary) {
    Civilization *saved_civs = malloc(sizeof(Civilization) * MAX_CIVS);
    City *saved_cities = malloc(sizeof(City) * MAX_CITIES);
    PlagueModelState *saved_model = malloc(sizeof(PlagueModelState));
    SnapshotPlagueImpact *impact = calloc(1, sizeof(SnapshotPlagueImpact));
    SnapshotPlagueImpact *zero = calloc(1, sizeof(SnapshotPlagueImpact));
    int saved_civ_count = civ_count;
    int saved_city_count = city_count;
    int sorted = 0;
    int top5 = 0;
    int fewer = 0;
    int unattributed = 0;
    int inactive = 0;
    int restored = 0;
    int ok;
    if (!saved_civs || !saved_cities || !saved_model || !impact || !zero) {
        free(saved_civs);
        free(saved_cities);
        free(saved_model);
        free(impact);
        free(zero);
        fprintf(summary, "case=plague_impact_snapshot ok=0 allocation=0\n");
        return 0;
    }
    memcpy(saved_civs, civs, sizeof(Civilization) * MAX_CIVS);
    memcpy(saved_cities, cities, sizeof(City) * MAX_CITIES);
    plague_state_copy(saved_model);
    fixture_setup();
    render_snapshot_plague_impact_build(impact, 1200);
    sorted = sorted_country_case(impact);
    top5 = top_city_case(impact);
    unattributed = unattributed_case(impact);
    fixture_setup();
    fewer = fewer_city_case(impact);
    inactive = inactive_clear_case(impact, zero);
    memcpy(civs, saved_civs, sizeof(Civilization) * MAX_CIVS);
    memcpy(cities, saved_cities, sizeof(City) * MAX_CITIES);
    civ_count = saved_civ_count;
    city_count = saved_city_count;
    restored = plague_state_restore(saved_model);
    ok = sorted && top5 && unattributed && fewer && inactive && restored;
    fprintf(summary,
            "case=plague_impact_snapshot ok=%d country_sort=%d tie_civ_id=%d current_owner_attribution=%d unattributed_accounting=%d recovered_visible=%d no_longer_exists_visible=%d top5=%d fewer_than5=%d inactive_clears=%d restored=%d\n",
            ok, sorted, sorted, top5, unattributed, top5, top5, top5, fewer,
            inactive, restored);
    free(saved_civs);
    free(saved_cities);
    free(saved_model);
    free(impact);
    free(zero);
    return ok;
}
