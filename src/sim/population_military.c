#include "sim/population_military.h"

#include "core/game_types.h"
#include "sim/population.h"
#include "sim/simulation.h"

typedef struct {
    PopulationCohort *cohort;
    int male;
    int weight;
} CasualtyBucket;

static int bucket_available(const CasualtyBucket *bucket) {
    return bucket->male ? bucket->cohort->male : bucket->cohort->female;
}

static int take_bucket(CasualtyBucket *bucket, int amount) {
    int *value = bucket->male ? &bucket->cohort->male : &bucket->cohort->female;
    int taken = clamp(amount, 0, *value);
    *value -= taken;
    return taken;
}

static int weighted_target(const CasualtyBucket *bucket, int casualties,
                           long long total_weight) {
    long long bucket_weight = (long long)bucket_available(bucket) * bucket->weight;
    if (bucket_weight <= 0 || total_weight <= 0) return 0;
    return (int)(casualties * bucket_weight / total_weight);
}

int population_military_base_soldiers_from_summary(PopulationSummary summary) {
    int male = summary.cohorts[POP_AGE_25_39].male +
               summary.cohorts[POP_AGE_40_54].male +
               summary.cohorts[POP_AGE_55_64].male;
    int female = summary.cohorts[POP_AGE_25_39].female +
                 summary.cohorts[POP_AGE_40_54].female +
                 summary.cohorts[POP_AGE_55_64].female;
    return (int)(((long long)male * 25 + (long long)female * 10 + 500) / 1000);
}

int population_military_base_soldiers_for_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    return population_military_base_soldiers_from_summary(
        population_country_summary(civ_id));
}

int population_military_current_soldiers_for_civ(int civ_id,
                                                 int active_war_casualties) {
    return max(0, population_military_base_soldiers_for_civ(civ_id) -
                  max(0, active_war_casualties));
}

void population_military_split_current_soldiers(PopulationSummary summary,
                                                int current_soldiers,
                                                int *male, int *female) {
    int male_recruitable = summary.cohorts[POP_AGE_25_39].male +
                           summary.cohorts[POP_AGE_40_54].male +
                           summary.cohorts[POP_AGE_55_64].male;
    int female_recruitable = summary.cohorts[POP_AGE_25_39].female +
                             summary.cohorts[POP_AGE_40_54].female +
                             summary.cohorts[POP_AGE_55_64].female;
    long long male_weight = (long long)male_recruitable * 25;
    long long female_weight = (long long)female_recruitable * 10;
    long long total_weight = male_weight + female_weight;
    current_soldiers = max(0, current_soldiers);
    if (male) *male = 0;
    if (female) *female = 0;
    if (total_weight <= 0) return;
    if (male) *male = (int)(((long long)current_soldiers * male_weight +
                             total_weight / 2) / total_weight);
    if (female) *female = current_soldiers - (male ? *male : 0);
}

int population_military_apply_casualties(int civ_id, int casualties) {
    int city_id;
    int removed = 0;
    if (civ_id < 0 || civ_id >= civ_count || casualties <= 0) return 0;
    for (city_id = 0; city_id < city_count && removed < casualties; city_id++) {
        CasualtyBucket buckets[6];
        long long total_weight = 0;
        int city_target = casualties - removed;
        int remaining = city_target;
        int i;
        if (!cities[city_id].alive || cities[city_id].owner != civ_id) continue;
        if (!cities[city_id].population_ready) population_init_city(city_id, cities[city_id].population);
        buckets[0] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_25_39], 1, 25};
        buckets[1] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_40_54], 1, 25};
        buckets[2] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_55_64], 1, 25};
        buckets[3] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_25_39], 0, 10};
        buckets[4] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_40_54], 0, 10};
        buckets[5] = (CasualtyBucket){&cities[city_id].population_cohorts[POP_AGE_55_64], 0, 10};
        for (i = 0; i < 6; i++) {
            total_weight += (long long)bucket_available(&buckets[i]) * buckets[i].weight;
        }
        if (total_weight <= 0) continue;
        for (i = 0; i < 6 && remaining > 0; i++) {
            int want = weighted_target(&buckets[i], city_target, total_weight);
            removed += take_bucket(&buckets[i], min(want, remaining));
            remaining = casualties - removed;
        }
        for (i = 0; i < 6 && removed < casualties; i++) {
            removed += take_bucket(&buckets[i], casualties - removed);
        }
        population_sync_city(city_id);
    }
    population_sync_all();
    world_invalidate_population_cache();
    return removed;
}
